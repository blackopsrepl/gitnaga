#include "git_client.hpp"
#include "git_staging.hpp"
#include "git_test_helpers.hpp"
#include "recent_projects.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include <ranges>

using namespace GitNaga;

class WorkInProgressTest final : public QObject
{
    Q_OBJECT

private slots:
    void readsWorkInProgressWithoutWritingObjects()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = directory.path();
        runGit(root, { QStringLiteral("init"), QStringLiteral("-b"), QStringLiteral("main") });
        runGit(root, { QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("GitNaga Test") });
        runGit(root, { QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@gitnaga.invalid") });

        writeFile(root + QStringLiteral("/tracked.txt"), QByteArrayLiteral("one\n"));
        runGit(root, { QStringLiteral("add"), QStringLiteral("tracked.txt") });
        runGit(root, { QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("initial"), QStringLiteral("tracked.txt") });

        // A clean worktree has no uncommitted work.
        auto clean = GitClient::loadRepository(root);
        QVERIFY(clean.has_value());
        QVERIFY(clean->status.files.isEmpty());
        QCOMPARE(clean->commits.size(), 1);

        // Stage an edit, edit the same file again, and add an untracked file.
        writeFile(root + QStringLiteral("/tracked.txt"), QByteArrayLiteral("two\n"));
        runGit(root, { QStringLiteral("add"), QStringLiteral("tracked.txt") });
        writeFile(root + QStringLiteral("/tracked.txt"), QByteArrayLiteral("three\n"));
        writeFile(root + QStringLiteral("/scratch.txt"), QByteArrayLiteral("scratch\n"));
        const auto objectsBefore = GitClient::mutate(root, { QStringLiteral("count-objects") }, QStringLiteral("count"));
        const auto indexBefore = QFileInfo(root + QStringLiteral("/.git/index")).lastModified();

        auto dirty = GitClient::loadRepository(root);
        QVERIFY(dirty.has_value());
        QCOMPARE(dirty->commits.size(), 1);
        QCOMPARE(dirty->status.files.size(), 2);
        const auto tracked = std::ranges::find_if(dirty->status.files, [](const FileChange &file) {
            return file.path == QStringLiteral("tracked.txt");
        });
        QVERIFY(tracked != dirty->status.files.end());
        QVERIFY(tracked->staged && tracked->unstaged && !tracked->untracked);
        const auto scratch = std::ranges::find_if(dirty->status.files, [](const FileChange &file) {
            return file.path == QStringLiteral("scratch.txt");
        });
        QVERIFY(scratch != dirty->status.files.end());
        QVERIFY(scratch->untracked);

        // The worktree diff is the working change against HEAD, not HEAD's
        // own diff: "one" goes, "three" arrives, the staged "two" is between.
        const auto trackedDiff = GitClient::loadDiff(root, dirty->refs.headOid, QStringLiteral("tracked.txt"), {},
                                                     DiffSide::Worktree);
        QVERIFY(trackedDiff.has_value());
        QVERIFY(std::ranges::any_of(*trackedDiff, [](const DiffLine &line) {
            return line.kind == DiffLine::Kind::Addition && line.text == QStringLiteral("+three");
        }));
        QVERIFY(std::ranges::any_of(*trackedDiff, [](const DiffLine &line) {
            return line.kind == DiffLine::Kind::Deletion && line.text == QStringLiteral("-one");
        }));
        const auto staged = GitClient::loadDiff(root, dirty->refs.headOid, QStringLiteral("tracked.txt"), {},
                                                DiffSide::Staged);
        QVERIFY(staged.has_value());
        QVERIFY(std::ranges::any_of(*staged, [](const DiffLine &line) { return line.text == QStringLiteral("+two"); }));
        const auto unstaged = GitClient::loadDiff(root, dirty->refs.headOid, QStringLiteral("tracked.txt"), {},
                                                  DiffSide::Unstaged);
        QVERIFY(unstaged.has_value());
        QVERIFY(std::ranges::any_of(*unstaged, [](const DiffLine &line) { return line.text == QStringLiteral("-two"); }));

        const auto untrackedDiff = GitClient::loadDiff(root, dirty->refs.headOid, QStringLiteral("scratch.txt"), {},
                                                       DiffSide::Worktree, true);
        QVERIFY(untrackedDiff.has_value());
        QVERIFY(std::ranges::any_of(*untrackedDiff, [](const DiffLine &line) {
            return line.kind == DiffLine::Kind::Addition && line.text == QStringLiteral("+scratch");
        }));

        // Reading the worktree writes nothing: no objects, no index refresh,
        // and the staged edit and untracked file keep their state.
        const auto objectsAfter = GitClient::mutate(root, { QStringLiteral("count-objects") }, QStringLiteral("count"));
        QCOMPARE(*objectsAfter, *objectsBefore);
        QCOMPARE(QFileInfo(root + QStringLiteral("/.git/index")).lastModified(), indexBefore);
        const auto status = GitClient::mutate(root, { QStringLiteral("status"), QStringLiteral("--porcelain") },
                                              QStringLiteral("read status"));
        QVERIFY(status.has_value());
        QCOMPARE(status->trimmed(), QStringLiteral("MM tracked.txt") + QLatin1Char('\n')
                                       + QStringLiteral("?? scratch.txt"));
    }

    void commitsTheIndexAndAmends()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = directory.path();
        runGit(root, { QStringLiteral("init"), QStringLiteral("-b"), QStringLiteral("main") });
        runGit(root, { QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("GitNaga Test") });
        runGit(root, { QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@gitnaga.invalid") });
        writeFile(root + QStringLiteral("/tracked.txt"), QByteArrayLiteral("one\n"));
        runGit(root, { QStringLiteral("add"), QStringLiteral("tracked.txt") });
        runGit(root, { QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("initial") });

        // A staged edit with a further unstaged edit on top, and an untracked
        // file: the file edited twice is on both sides.
        writeFile(root + QStringLiteral("/tracked.txt"), QByteArrayLiteral("two\n"));
        runGit(root, { QStringLiteral("add"), QStringLiteral("tracked.txt") });
        writeFile(root + QStringLiteral("/tracked.txt"), QByteArrayLiteral("three\n"));
        writeFile(root + QStringLiteral("/scratch.txt"), QByteArrayLiteral("scratch\n"));
        const auto status = GitClient::loadStatus(*GitClient::locate(root));
        QVERIFY(status.has_value());
        QVector<FileChange> unstaged;
        QVector<FileChange> staged;
        staging::splitStatus(status->files, &unstaged, &staged);
        QCOMPARE(unstaged.size(), 2);
        QCOMPARE(staged.size(), 1);
        QCOMPARE(staged.front().path, QStringLiteral("tracked.txt"));

        // The commit takes the index only: "two", not "three" or scratch.
        const auto committed = staging::commit(root, QStringLiteral("wip commit"), QString(), false);
        QVERIFY2(committed.has_value(), qPrintable(committed ? QString() : committed.error().message));
        const auto contents = GitClient::mutate(root, { QStringLiteral("show"), QStringLiteral("HEAD:tracked.txt") },
                                                QStringLiteral("read committed content"));
        QCOMPARE(contents->trimmed(), QStringLiteral("two"));
        const auto porcelain = GitClient::mutate(root, { QStringLiteral("status"), QStringLiteral("--porcelain") },
                                                 QStringLiteral("read status"));
        QCOMPARE(porcelain->trimmed(), QStringLiteral("M tracked.txt\n?? scratch.txt"));

        // Amending with nothing staged rewrites the message only.
        const auto head = staging::headMessage(root);
        QVERIFY(head.has_value());
        QCOMPARE(head->summary, QStringLiteral("wip commit"));
        QVERIFY(!head->published);
        const auto amended = staging::commit(root, QStringLiteral("better"), QStringLiteral("body"), true);
        QVERIFY(amended.has_value());
        QVERIFY(*amended != *committed);
        const auto log = GitClient::mutate(root, { QStringLiteral("log"), QStringLiteral("--format=%s|%b") },
                                           QStringLiteral("read log"));
        QCOMPARE(log->trimmed(), QStringLiteral("better|body\n\ninitial|"));
    }

    void remembersProjectsAndFuzzyMatches()
    {
        const QString settings = QDir::temp().filePath(QStringLiteral("gitnaga-recents-test.ini"));
        QFile::remove(settings);

        RecentProjects::record(QStringLiteral("/repos/alpha"));
        RecentProjects::record(QStringLiteral("/repos/beta"));
        RecentProjects::record(QStringLiteral("/repos/alpha"));  // dedupe, move to front

        QCOMPARE(RecentProjects::load().size(), 2);
        QCOMPARE(RecentProjects::load().front(), QStringLiteral("/repos/alpha"));

        // Subsequence matching: "bta" matches "beta"; "zzz" matches nothing.
        const auto betaMatches = RecentProjects::fuzzyMatch(QStringLiteral("bta"));
        QCOMPARE(betaMatches.size(), 1);
        QCOMPARE(betaMatches.front().toMap().value(QStringLiteral("path")).toString(), QStringLiteral("/repos/beta"));
        QCOMPARE(RecentProjects::fuzzyMatch(QStringLiteral("zzz")).size(), 0);

        // An empty needle ranks every project without dropping any.
        QCOMPARE(RecentProjects::fuzzyMatch(QString()).size(), 2);

        QFile::remove(settings);
    }
};

QTEST_GUILESS_MAIN(WorkInProgressTest)

#include "wip_test.moc"
