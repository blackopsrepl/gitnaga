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
};

QTEST_GUILESS_MAIN(GitClientTest)

#include "git_client_test.moc"
