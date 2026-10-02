#include "git_client.hpp"
#include "git_test_helpers.hpp"

#include <QDir>
#include <QProcess>
#include <QTest>

// Deleting a remote branch works against any git server, so it is proven
// against real ones: a bare repository on disk stands in for any transport
// (the push is the same git push over https, ssh, git, or file), and the
// local Forgejo runs the same path over http when it is reachable.

using namespace GitNaga;

namespace {

QString bareRemote(const QTemporaryDir &directory, const QString &name)
{
    const auto path = QDir(directory.path()).filePath(name);
    runGit(directory.path(), { QStringLiteral("init"), QStringLiteral("--bare"), path });
    return path;
}

QString scratchRepository(const QTemporaryDir &directory, const QString &name)
{
    const auto root = QDir(directory.path()).filePath(name);
    runGit(directory.path(), { QStringLiteral("init"), QStringLiteral("-b"), QStringLiteral("main"), root });
    runGit(root, { QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("GitNaga Test") });
    runGit(root, { QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@gitnaga.invalid") });
    writeFile(root + QStringLiteral("/readme.txt"), QByteArrayLiteral("hello\n"));
    runGit(root, { QStringLiteral("add"), QStringLiteral("readme.txt") });
    runGit(root, { QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("initial") });
    return root;
}

QStringList linesOf(const QString &directory, const QStringList &arguments)
{
    const auto output = GitClient::mutate(directory, arguments, QStringLiteral("list refs"));
    Q_ASSERT(output.has_value());
    return output->split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

QStringList remoteBranches(const QString &bare)
{
    return linesOf(bare, { QStringLiteral("for-each-ref"), QStringLiteral("--format=%(refname:short)"),
                           QStringLiteral("refs/heads") });
}

QStringList trackingRefs(const QString &root)
{
    return linesOf(root, { QStringLiteral("for-each-ref"), QStringLiteral("--format=%(refname:short)"),
                           QStringLiteral("refs/remotes") });
}

// One repository with a bare remote, main pushed, and the named branches
// published. Returns the worktree root.
QString publishedRepository(const QTemporaryDir &directory, const QStringList &branches)
{
    const auto bare = bareRemote(directory, QStringLiteral("remote.git"));
    const auto root = scratchRepository(directory, QStringLiteral("work"));
    runGit(root, { QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("origin"), bare });
    runGit(root, { QStringLiteral("push"), QStringLiteral("-q"), QStringLiteral("-u"),
                   QStringLiteral("origin"), QStringLiteral("main") });
    for (const auto &branch : branches) {
        runGit(root, { QStringLiteral("branch"), branch });
        runGit(root, { QStringLiteral("push"), QStringLiteral("-q"), QStringLiteral("origin"), branch });
    }
    return root;
}

} // namespace

class GitRemoteTest final : public QObject
{
    Q_OBJECT

private slots:
    void refusesEmptyNames()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = scratchRepository(directory, QStringLiteral("work"));
        QVERIFY(!GitClient::deleteRemoteBranch(root, QString(), QStringLiteral("feature")).has_value());
        QVERIFY(!GitClient::deleteRemoteBranch(root, QStringLiteral("origin"), QString()).has_value());
    }

    void refusesAMissingRemote()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = scratchRepository(directory, QStringLiteral("work"));

        const auto result = GitClient::deleteRemoteBranch(root, QStringLiteral("nope"),
                                                          QStringLiteral("feature"));
        QVERIFY(!result.has_value());
        // git's own message names the cause; nothing is invented here.
        QVERIFY2(result.error().message.contains(QStringLiteral("nope")),
                 qPrintable(result.error().message));
    }

    void deletesOnAnyGitServer()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto bare = QDir(directory.path()).filePath(QStringLiteral("remote.git"));
        const auto root = publishedRepository(directory, { QStringLiteral("feature") });

        QVERIFY(remoteBranches(bare).contains(QStringLiteral("feature")));
        QVERIFY(trackingRefs(root).contains(QStringLiteral("origin/feature")));

        QVERIFY2(GitClient::deleteRemoteBranch(root, QStringLiteral("origin"), QStringLiteral("feature")).has_value(),
                 "delete against a plain git server");

        // Gone on the server, and git pruned the local remote-tracking ref.
        QVERIFY(!remoteBranches(bare).contains(QStringLiteral("feature")));
        QVERIFY(!trackingRefs(root).contains(QStringLiteral("origin/feature")));
        // The main branch and its tracking ref are untouched.
        QVERIFY(remoteBranches(bare).contains(QStringLiteral("main")));
        QVERIFY(trackingRefs(root).contains(QStringLiteral("origin/main")));
    }

    void deletesABranchWithNoLocalBranch()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto bare = QDir(directory.path()).filePath(QStringLiteral("remote.git"));
        const auto root = publishedRepository(directory, { QStringLiteral("remote-only") });
        // Delete the local branch, keeping only the remote-tracking ref: the
        // sidebar still offers it, from a ref with no local branch behind it.
        runGit(root, { QStringLiteral("branch"), QStringLiteral("-D"), QStringLiteral("remote-only") });
        QVERIFY(trackingRefs(root).contains(QStringLiteral("origin/remote-only")));

        // The full refname is what makes this work: a short name is resolved
        // locally and fails for exactly this case.
        QVERIFY2(GitClient::deleteRemoteBranch(root, QStringLiteral("origin"),
                                               QStringLiteral("remote-only")).has_value(),
                 "delete a remote-tracking ref that has no local branch");
        QVERIFY(!remoteBranches(bare).contains(QStringLiteral("remote-only")));
        QVERIFY(!trackingRefs(root).contains(QStringLiteral("origin/remote-only")));
    }

    void deletesANestedBranchName()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto bare = QDir(directory.path()).filePath(QStringLiteral("remote.git"));
        const auto root = publishedRepository(directory, {});
        runGit(root, { QStringLiteral("push"), QStringLiteral("-q"), QStringLiteral("origin"),
                       QStringLiteral("main:refs/heads/feature/deep-name") });
        QVERIFY(remoteBranches(bare).contains(QStringLiteral("feature/deep-name")));

        QVERIFY(GitClient::deleteRemoteBranch(root, QStringLiteral("origin"),
                                              QStringLiteral("feature/deep-name")).has_value());
        QVERIFY(!remoteBranches(bare).contains(QStringLiteral("feature/deep-name")));
    }

    void reportsAServerThatRefusesDeletions()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto bare = QDir(directory.path()).filePath(QStringLiteral("remote.git"));
        const auto root = publishedRepository(directory, { QStringLiteral("protected") });
        runGit(bare, { QStringLiteral("config"), QStringLiteral("receive.denyDeletes"),
                       QStringLiteral("true") });

        const auto result = GitClient::deleteRemoteBranch(root, QStringLiteral("origin"),
                                                          QStringLiteral("protected"));
        QVERIFY(!result.has_value());
        QVERIFY2(result.error().message.contains(QStringLiteral("deny")),
                 qPrintable(result.error().message));
        // The refusal removed nothing, locally or on the server.
        QVERIFY(remoteBranches(bare).contains(QStringLiteral("protected")));
        QVERIFY(trackingRefs(root).contains(QStringLiteral("origin/protected")));
    }

    void treatsAnAlreadyDeletedBranchAsDone()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        publishedRepository(directory, {});

        // A branch that never existed on the server: git warns and exits zero,
        // the honest answer for a delete that is already satisfied.
        const auto root = QDir(directory.path()).filePath(QStringLiteral("work"));
        QVERIFY(GitClient::deleteRemoteBranch(root, QStringLiteral("origin"),
                                              QStringLiteral("never-was")).has_value());
    }

    void deletesOnTheLocalForgejo()
    {
        // The same code path, over http, against a real forge that is not
        // GitHub: a throwaway branch is created on the local Forgejo, deleted
        // through the code under test, and confirmed gone. Skipped when the
        // Forgejo is not reachable or does not accept a push from here.
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = scratchRepository(directory, QStringLiteral("work"));
        runGit(root, { QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("origin"),
                       QStringLiteral("http://vigilance:3002/blackopsrepl/gitnaga.git") });

        const auto probe = QStringLiteral("gitnaga-test-delete-%1").arg(QCoreApplication::applicationPid());
        const auto listed = [&root, &probe] {
            QProcess ls;
            ls.setWorkingDirectory(root);
            ls.start(QStringLiteral("git"), { QStringLiteral("ls-remote"), QStringLiteral("--heads"),
                                              QStringLiteral("origin"),
                                              QStringLiteral("refs/heads/%1").arg(probe) });
            if (!ls.waitForFinished(15000) || ls.exitCode() != 0)
                return QStringLiteral("<unreachable>");
            return QString::fromUtf8(ls.readAllStandardOutput()).trimmed();
        };

        // Reachability plus permission, in one read-only round trip.
        const auto initial = listed();
        if (initial == QStringLiteral("<unreachable>"))
            QSKIP("local Forgejo is not reachable from here");

        QProcess create;
        create.setWorkingDirectory(root);
        create.start(QStringLiteral("git"), { QStringLiteral("push"), QStringLiteral("origin"),
                                              QStringLiteral("HEAD:refs/heads/%1").arg(probe) });
        if (!create.waitForFinished(30000) || create.exitCode() != 0)
            QSKIP("local Forgejo does not accept a push from here");

        // The branch exists on the server before the delete.
        QVERIFY(!listed().isEmpty());

        QVERIFY2(GitClient::deleteRemoteBranch(root, QStringLiteral("origin"), probe).has_value(),
                 "delete on the local Forgejo");
        QVERIFY2(listed().isEmpty(), "the branch is gone on the server");
    }
};

QTEST_GUILESS_MAIN(GitRemoteTest)

#include "git_remote_test.moc"
