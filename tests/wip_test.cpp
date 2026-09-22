#include "git_client.hpp"
#include "git_test_helpers.hpp"
#include "recent_projects.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <ranges>

using namespace GitNaga;

class WorkInProgressTest final : public QObject
{
    Q_OBJECT

private slots:
    void snapshotsWorkInProgress()
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

        // A clean worktree produces no snapshot at all.
        auto clean = GitClient::loadRepository(root);
        QVERIFY(clean.has_value());
        QVERIFY(clean->workInProgressOid.isEmpty());
        QCOMPARE(clean->commits.size(), 1);

        // Stage an edit, edit the same file again, and add an untracked file.
        writeFile(root + QStringLiteral("/tracked.txt"), QByteArrayLiteral("two\n"));
        runGit(root, { QStringLiteral("add"), QStringLiteral("tracked.txt") });
        writeFile(root + QStringLiteral("/tracked.txt"), QByteArrayLiteral("three\n"));
        writeFile(root + QStringLiteral("/scratch.txt"), QByteArrayLiteral("scratch\n"));

        const auto head = GitClient::mutate(root, { QStringLiteral("rev-parse"), QStringLiteral("HEAD") },
                                            QStringLiteral("read HEAD"));
        QVERIFY(head.has_value());

        auto dirty = GitClient::loadRepository(root);
        QVERIFY(dirty.has_value());
        QVERIFY(!dirty->workInProgressOid.isEmpty());
        QCOMPARE(dirty->commits.size(), 2);

        // The snapshot is a real, resolvable commit whose parent is HEAD.
        const auto &snapshotCommit = dirty->commits.first();
        QCOMPARE(snapshotCommit.oid, dirty->workInProgressOid);
        QCOMPARE(snapshotCommit.parents.size(), 1);
        QCOMPARE(snapshotCommit.parents.first(), head->trimmed());
        QCOMPARE(snapshotCommit.author, QStringLiteral("GitNaga Test"));
        QVERIFY(GitClient::mutate(root, { QStringLiteral("cat-file"), QStringLiteral("-e"),
                                          snapshotCommit.oid }, QStringLiteral("verify snapshot")).has_value());

        // Inspection and diffs run through the ordinary commit path, including
        // for the untracked file.
        const auto inspection = GitClient::inspectCommit(root, snapshotCommit.oid);
        QVERIFY(inspection.has_value());
        QCOMPARE(inspection->details.subject, QStringLiteral("Work in progress"));
        QCOMPARE(inspection->files.size(), 2);
        QVERIFY(std::ranges::any_of(inspection->files, [](const FileChange &file) {
            return file.path == QStringLiteral("scratch.txt") && file.status == QStringLiteral("A");
        }));

        const auto trackedDiff = GitClient::loadDiff(root, snapshotCommit.oid, QStringLiteral("tracked.txt"));
        QVERIFY(trackedDiff.has_value());
        QVERIFY(std::ranges::any_of(*trackedDiff, [](const DiffLine &line) {
            return line.kind == DiffLine::Kind::Addition && line.text.contains(QStringLiteral("three"));
        }));

        const auto untrackedDiff = GitClient::loadDiff(root, snapshotCommit.oid, QStringLiteral("scratch.txt"));
        QVERIFY(untrackedDiff.has_value());
        QVERIFY(std::ranges::any_of(*untrackedDiff, [](const DiffLine &line) {
            return line.kind == DiffLine::Kind::Addition && line.text.contains(QStringLiteral("scratch"));
        }));

        // Taking the snapshot must leave the repository's own index and
        // worktree untouched: the staged edit is still staged (MM) and the
        // untracked file is still untracked.
        const auto status = GitClient::mutate(root, { QStringLiteral("status"), QStringLiteral("--porcelain") },
                                              QStringLiteral("read status"));
        QVERIFY(status.has_value());
        QCOMPARE(status->trimmed(), QStringLiteral("MM tracked.txt") + QLatin1Char('\n')
                                       + QStringLiteral("?? scratch.txt"));
    }

    void commitsSelectedWorktreeFiles()
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

        // staged edit of tracked.txt, then a further unstaged edit; a staged
        // new file; and an untracked file the user wants included.
        writeFile(root + QStringLiteral("/tracked.txt"), QByteArrayLiteral("two\n"));
        runGit(root, { QStringLiteral("add"), QStringLiteral("tracked.txt") });
        writeFile(root + QStringLiteral("/tracked.txt"), QByteArrayLiteral("three\n"));
        writeFile(root + QStringLiteral("/staged.txt"), QByteArrayLiteral("staged\n"));
        runGit(root, { QStringLiteral("add"), QStringLiteral("staged.txt") });
        writeFile(root + QStringLiteral("/scratch.txt"), QByteArrayLiteral("scratch\n"));

        const auto result = GitClient::commitWorktree(root,
                                                      { QStringLiteral("tracked.txt"), QStringLiteral("scratch.txt") },
                                                      { QStringLiteral("staged.txt") },
                                                      QStringLiteral("wip commit"), QString());
        QVERIFY2(result.has_value(), qPrintable(result.has_value() ? QString()
            : result.error().operation + QStringLiteral(": ") + result.error().message));

        auto head = GitClient::mutate(root, { QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("HEAD") },
                                      QStringLiteral("read HEAD"));
        QVERIFY(head.has_value());
        QCOMPARE(head->trimmed(), result->trimmed());

        const auto names = GitClient::mutate(root,
                                             { QStringLiteral("show"), QStringLiteral("--name-status"),
                                               QStringLiteral("--format="), QStringLiteral("HEAD") },
                                             QStringLiteral("list committed files"));
        QVERIFY(names.has_value());
        QVERIFY(names->contains(QStringLiteral("tracked.txt")));
        QVERIFY(names->contains(QStringLiteral("scratch.txt")));
        QVERIFY(!names->contains(QStringLiteral("staged.txt")));

        const auto contents = GitClient::mutate(root, { QStringLiteral("show"), QStringLiteral("HEAD:tracked.txt") },
                                                QStringLiteral("read committed content"));
        QVERIFY(contents.has_value());
        QCOMPARE(contents->trimmed(), QStringLiteral("three"));

        // The excluded file keeps its worktree state, unstaged; the included
        // ones are committed and clean.
        const auto status = GitClient::mutate(root, { QStringLiteral("status"), QStringLiteral("--porcelain") },
                                              QStringLiteral("read status"));
        QVERIFY(status.has_value());
        QCOMPARE(status->trimmed(), QStringLiteral("?? staged.txt"));
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
