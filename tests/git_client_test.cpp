#include "git_client.hpp"
#include "git_test_helpers.hpp"

#include <QTest>

#include <ranges>

using namespace GitNaga;
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
