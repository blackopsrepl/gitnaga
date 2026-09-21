#include "git_client.hpp"

#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

using namespace GitNaga;

namespace {

void runGit(const QString &directory, const QStringList &arguments)
{
    QProcess process;
    process.setWorkingDirectory(directory);
    process.start(QStringLiteral("git"), arguments);
    QVERIFY2(process.waitForFinished(), "git command timed out");
    QCOMPARE(process.exitCode(), 0);
}

void writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(contents), contents.size());
}

} // namespace

class GitClientTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadsTopologyAndInspectsMerge()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = directory.path();
        runGit(root, { QStringLiteral("init"), QStringLiteral("-b"), QStringLiteral("main") });
        runGit(root, { QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("GitNaga Test") });
        runGit(root, { QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@gitnaga.invalid") });

        writeFile(root + QStringLiteral("/base.txt"), QByteArrayLiteral("base\n"));
        runGit(root, { QStringLiteral("add"), QStringLiteral("base.txt") });
        runGit(root, { QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("base") });
        runGit(root, { QStringLiteral("switch"), QStringLiteral("-c"), QStringLiteral("feature") });
        writeFile(root + QStringLiteral("/feature.txt"), QByteArrayLiteral("feature\n"));
        runGit(root, { QStringLiteral("add"), QStringLiteral("feature.txt") });
        runGit(root, { QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("feature") });
        runGit(root, { QStringLiteral("switch"), QStringLiteral("main") });
        writeFile(root + QStringLiteral("/main.txt"), QByteArrayLiteral("main\n"));
        runGit(root, { QStringLiteral("add"), QStringLiteral("main.txt") });
        runGit(root, { QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("main work") });
        runGit(root, { QStringLiteral("merge"), QStringLiteral("--no-ff"), QStringLiteral("feature"),
                       QStringLiteral("-m"), QStringLiteral("merge feature") });

        const auto snapshot = GitClient::loadRepository(root);
        QVERIFY(snapshot.has_value());
        QCOMPARE(snapshot->currentBranch, QStringLiteral("main"));
        QCOMPARE(snapshot->commits.size(), 4);
        QCOMPARE(snapshot->commits.first().parents.size(), 2);
        QVERIFY(snapshot->commits.first().laneCount >= 2);
        QVERIFY(snapshot->commits.first().refs.contains(QStringLiteral("main")));

        const auto inspection = GitClient::inspectCommit(root, snapshot->commits.first().oid);
        QVERIFY(inspection.has_value());
        QCOMPARE(inspection->details.subject, QStringLiteral("merge feature"));
        QCOMPARE(inspection->files.size(), 1);
        QCOMPARE(inspection->files.first().path, QStringLiteral("feature.txt"));

        const auto diff = GitClient::loadDiff(root, snapshot->commits.first().oid, QStringLiteral("feature.txt"));
        QVERIFY(diff.has_value());
        QVERIFY(std::ranges::any_of(*diff, [](const DiffLine &line) {
            return line.kind == DiffLine::Kind::Addition && line.text.contains(QStringLiteral("feature"));
        }));
    }

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
        runGit(root, { QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("initial") });

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

    void showsStagedRenameDiff()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = directory.path();
        runGit(root, { QStringLiteral("init"), QStringLiteral("-b"), QStringLiteral("main") });
        runGit(root, { QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("GitNaga Test") });
        runGit(root, { QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@gitnaga.invalid") });

        writeFile(root + QStringLiteral("/old-name.txt"),
                  QByteArrayLiteral("alpha\nbeta\ngamma\ndelta\n"));
        runGit(root, { QStringLiteral("add"), QStringLiteral("old-name.txt") });
        runGit(root, { QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("initial") });
        runGit(root, { QStringLiteral("mv"), QStringLiteral("old-name.txt"), QStringLiteral("new-name.txt") });

        const auto snapshot = GitClient::loadRepository(root);
        QVERIFY(snapshot.has_value());
        QVERIFY(!snapshot->workInProgressOid.isEmpty());

        const auto inspection = GitClient::inspectCommit(root, snapshot->workInProgressOid);
        QVERIFY(inspection.has_value());
        QCOMPARE(inspection->files.size(), 1);
        QCOMPARE(inspection->files.first().status, QStringLiteral("R"));
        QCOMPARE(inspection->files.first().oldPath, QStringLiteral("old-name.txt"));
        QCOMPARE(inspection->files.first().path, QStringLiteral("new-name.txt"));

        // Naming both sides keeps the diff a rename instead of an add.
        const auto diff = GitClient::loadDiff(root, snapshot->workInProgressOid,
                                              QStringLiteral("new-name.txt"), QStringLiteral("old-name.txt"));
        QVERIFY(diff.has_value());
        QVERIFY(std::ranges::none_of(*diff, [](const DiffLine &line) {
            return line.kind == DiffLine::Kind::Deletion;
        }));
    }

    void performsBranchTagAndResetOperations()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = directory.path();
        runGit(root, { QStringLiteral("init"), QStringLiteral("-b"), QStringLiteral("main") });
        runGit(root, { QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("GitNaga Test") });
        runGit(root, { QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@gitnaga.invalid") });

        writeFile(root + QStringLiteral("/readme.txt"), QByteArrayLiteral("hello\n"));
        runGit(root, { QStringLiteral("add"), QStringLiteral("readme.txt") });
        runGit(root, { QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("initial") });

        const auto head = GitClient::mutate(root, { QStringLiteral("rev-parse"), QStringLiteral("HEAD") },
                                            QStringLiteral("read HEAD"));
        QVERIFY(head.has_value());
        const auto base = head->trimmed();

        QVERIFY(GitClient::mutate(root, { QStringLiteral("branch"), QStringLiteral("feature"), base },
                                  QStringLiteral("create branch")).has_value());
        QVERIFY(GitClient::mutate(root, { QStringLiteral("tag"), QStringLiteral("v-test"), base },
                                  QStringLiteral("create tag")).has_value());

        const auto branches = GitClient::mutate(root, { QStringLiteral("branch"), QStringLiteral("--list"),
                                                        QStringLiteral("feature") }, QStringLiteral("list branches"));
        QVERIFY(branches.has_value());
        QVERIFY(branches->contains(QStringLiteral("feature")));

        QVERIFY(GitClient::mutate(root, { QStringLiteral("checkout"), QStringLiteral("feature") },
                                  QStringLiteral("switch branch")).has_value());
        auto snapshot = GitClient::loadRepository(root);
        QVERIFY(snapshot.has_value());
        QCOMPARE(snapshot->currentBranch, QStringLiteral("feature"));

        writeFile(root + QStringLiteral("/readme.txt"), QByteArrayLiteral("hello\nfeature\n"));
        runGit(root, { QStringLiteral("commit"), QStringLiteral("-am"), QStringLiteral("feature work") });
        const auto featureHead = GitClient::mutate(root, { QStringLiteral("rev-parse"), QStringLiteral("HEAD") },
                                                   QStringLiteral("read HEAD"));
        QVERIFY(featureHead.has_value());
        QVERIFY(featureHead->trimmed() != base);

        QVERIFY(GitClient::mutate(root, { QStringLiteral("reset"), QStringLiteral("--hard"), base },
                                  QStringLiteral("reset branch")).has_value());
        const auto resetHead = GitClient::mutate(root, { QStringLiteral("rev-parse"), QStringLiteral("HEAD") },
                                                 QStringLiteral("read HEAD"));
        QVERIFY(resetHead.has_value());
        QCOMPARE(resetHead->trimmed(), base);

        QVERIFY(GitClient::mutate(root, { QStringLiteral("checkout"), QStringLiteral("main") },
                                  QStringLiteral("switch branch")).has_value());
        QVERIFY(GitClient::mutate(root, { QStringLiteral("branch"), QStringLiteral("-D"), QStringLiteral("feature") },
                                  QStringLiteral("delete branch")).has_value());
        const auto remaining = GitClient::mutate(root, { QStringLiteral("branch"), QStringLiteral("--list"),
                                                         QStringLiteral("feature") }, QStringLiteral("list branches"));
        QVERIFY(remaining.has_value());
        QVERIFY(remaining->trimmed().isEmpty());
    }
};

QTEST_GUILESS_MAIN(GitClientTest)

#include "git_client_test.moc"
