#include "git_client.hpp"
#include "git_sync.hpp"
#include "git_test_helpers.hpp"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

// Fetch, pull and push against a real bare remote on disk: the same git
// commands GitNaga runs over https or ssh, minus the network.

using namespace GitNaga;

namespace {

QString clone(const QString &root, const QString &bare, const QString &name)
{
    const auto path = QDir(root).filePath(name);
    runGit(root, { QStringLiteral("clone"), QStringLiteral("-q"), bare, path });
    runGit(path, { QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("GitNaga Test") });
    runGit(path, { QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@gitnaga.invalid") });
    return path;
}

void commit(const QString &repository, const QString &file, const QByteArray &line)
{
    writeFile(QDir(repository).filePath(file), line);
    runGit(repository, { QStringLiteral("add"), file });
    runGit(repository, { QStringLiteral("commit"), QStringLiteral("-qm"), QString::fromUtf8(line).trimmed() });
}

WorktreeStatus status(const QString &repository)
{
    auto location = GitClient::locate(repository);
    Q_ASSERT(location.has_value());
    auto loaded = GitClient::loadStatus(*location);
    Q_ASSERT(loaded.has_value());
    return *loaded;
}

QString revParse(const QString &repository, const QString &revision)
{
    return *GitClient::mutate(repository, { QStringLiteral("rev-parse"), revision }, QStringLiteral("rev-parse"));
}

// The failure text for a QVERIFY2 description, which is built even when the
// result holds a value.
template<typename T>
QByteArray why(const GitResult<T> &result)
{
    return result ? QByteArray("ok") : result.error().message.toUtf8();
}

// A bare remote with one commit on main and two clones of it, the second
// standing for a colleague (or this user's other machine).
struct Fixture {
    QTemporaryDir directory;
    QString bare;
    QString mine;
    QString theirs;

    Fixture()
    {
        bare = QDir(directory.path()).filePath(QStringLiteral("remote.git"));
        const auto seed = QDir(directory.path()).filePath(QStringLiteral("seed"));
        runGit(directory.path(), { QStringLiteral("init"), QStringLiteral("-q"), QStringLiteral("-b"), QStringLiteral("main"), seed });
        runGit(seed, { QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("Seed") });
        runGit(seed, { QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("seed@gitnaga.invalid") });
        commit(seed, QStringLiteral("readme.txt"), QByteArrayLiteral("one\n"));
        runGit(directory.path(), { QStringLiteral("clone"), QStringLiteral("-q"), QStringLiteral("--bare"), seed, bare });
        mine = clone(directory.path(), bare, QStringLiteral("mine"));
        theirs = clone(directory.path(), bare, QStringLiteral("theirs"));
    }

    void theyPush(const QByteArray &line)
    {
        commit(theirs, QStringLiteral("theirs.txt"), line);
        runGit(theirs, { QStringLiteral("push"), QStringLiteral("-q"), QStringLiteral("origin"), QStringLiteral("main") });
    }
};

} // namespace

class GitSyncTest final : public QObject
{
    Q_OBJECT

private slots:
    void fetchUpdatesTheBehindCount()
    {
        Fixture fixture;
        fixture.theyPush(QByteArrayLiteral("two\n"));
        QCOMPARE(status(fixture.mine).behind, 0);

        QStringList progress;
        RunOptions options;
        options.progress = [&progress](const QString &line) { progress.append(line); };
        const auto fetched = gitsync::fetchAll(fixture.mine, options);
        QVERIFY2(fetched.has_value(), why(fetched).constData());
        QCOMPARE(status(fixture.mine).behind, 1);
        QCOMPARE(status(fixture.mine).upstream, QStringLiteral("origin/main"));
    }

    void fetchPrunesDeletedBranches()
    {
        Fixture fixture;
        runGit(fixture.theirs, { QStringLiteral("push"), QStringLiteral("-q"), QStringLiteral("origin"), QStringLiteral("main:gone") });
        QVERIFY(gitsync::fetchAll(fixture.mine).has_value());
        QVERIFY(!revParse(fixture.mine, QStringLiteral("origin/gone")).isEmpty());
        runGit(fixture.theirs, { QStringLiteral("push"), QStringLiteral("-q"), QStringLiteral("origin"), QStringLiteral(":gone") });
        QVERIFY(gitsync::fetchAll(fixture.mine).has_value());
        QVERIFY(!GitClient::mutate(fixture.mine, { QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("origin/gone") },
                                   QStringLiteral("rev-parse")).has_value());
    }

    void pullFastForwards()
    {
        Fixture fixture;
        fixture.theyPush(QByteArrayLiteral("two\n"));
        const auto pulled = gitsync::pull(fixture.mine, gitsync::PullMode::FastForwardOnly);
        QVERIFY2(pulled.has_value(), why(pulled).constData());
        QCOMPARE(revParse(fixture.mine, QStringLiteral("HEAD")), revParse(fixture.theirs, QStringLiteral("HEAD")));
        QCOMPARE(status(fixture.mine).behind, 0);
    }

    void fastForwardOnlyRefusesDivergence()
    {
        Fixture fixture;
        fixture.theyPush(QByteArrayLiteral("two\n"));
        commit(fixture.mine, QStringLiteral("mine.txt"), QByteArrayLiteral("local\n"));
        const auto before = revParse(fixture.mine, QStringLiteral("HEAD"));
        QVERIFY(!gitsync::pull(fixture.mine, gitsync::PullMode::FastForwardOnly).has_value());
        QCOMPARE(revParse(fixture.mine, QStringLiteral("HEAD")), before);

        // The default mode merges instead; rebase replays the local commit.
        QVERIFY(gitsync::pull(fixture.mine, gitsync::PullMode::FastForwardOrMerge).has_value());
        QCOMPARE(revParse(fixture.mine, QStringLiteral("HEAD^2")), revParse(fixture.theirs, QStringLiteral("HEAD")));
    }

    void pullRebaseReplaysLocalWork()
    {
        Fixture fixture;
        fixture.theyPush(QByteArrayLiteral("two\n"));
        commit(fixture.mine, QStringLiteral("mine.txt"), QByteArrayLiteral("local\n"));
        QVERIFY(gitsync::pull(fixture.mine, gitsync::PullMode::Rebase).has_value());
        QCOMPARE(revParse(fixture.mine, QStringLiteral("HEAD~1")), revParse(fixture.theirs, QStringLiteral("HEAD")));
        QCOMPARE(status(fixture.mine).ahead, 1);
    }

    void pushPublishesANewBranchWithUpstream()
    {
        Fixture fixture;
        runGit(fixture.mine, { QStringLiteral("switch"), QStringLiteral("-qc"), QStringLiteral("topic") });
        commit(fixture.mine, QStringLiteral("topic.txt"), QByteArrayLiteral("topic\n"));
        QVERIFY(gitsync::trackingOf(fixture.mine, QStringLiteral("topic")).remote.isEmpty());

        const auto remotes = gitsync::listRemotes(fixture.mine);
        QVERIFY(remotes.has_value());
        QCOMPARE(gitsync::defaultRemote(*remotes), QStringLiteral("origin"));
        const auto pushed = gitsync::push(fixture.mine, { QStringLiteral("origin"), QStringLiteral("topic"), {}, true, false });
        QVERIFY2(pushed.has_value(), why(pushed).constData());

        QCOMPARE(status(fixture.mine).upstream, QStringLiteral("origin/topic"));
        const auto tracking = gitsync::trackingOf(fixture.mine, QStringLiteral("topic"));
        QCOMPARE(tracking.remote, QStringLiteral("origin"));
        QCOMPARE(tracking.branch, QStringLiteral("topic"));
        QCOMPARE(revParse(fixture.bare, QStringLiteral("topic")), revParse(fixture.mine, QStringLiteral("topic")));
    }

    void rejectedPushIsDetectedAndLeaseForces()
    {
        Fixture fixture;
        fixture.theyPush(QByteArrayLiteral("two\n"));
        commit(fixture.mine, QStringLiteral("mine.txt"), QByteArrayLiteral("local\n"));
        const gitsync::PushRequest plain{ QStringLiteral("origin"), QStringLiteral("main"), {}, false, false };

        // Not fetched yet: git says "fetch first", and the lease refuses too,
        // because the server moved past what this repository last saw.
        const auto unseen = gitsync::push(fixture.mine, plain);
        QVERIFY(!unseen.has_value());
        QVERIFY2(gitsync::isNonFastForward(unseen.error().message), why(unseen).constData());
        auto lease = plain;
        lease.forceWithLease = true;
        const auto stale = gitsync::push(fixture.mine, lease);
        QVERIFY(!stale.has_value());
        QVERIFY2(stale.error().message.contains(QStringLiteral("stale info")), why(stale).constData());
        QVERIFY(!gitsync::isNonFastForward(QStringLiteral("remote: hook declined")));

        // Seen: still a non-fast-forward, and now the lease replaces it.
        QVERIFY(gitsync::fetchAll(fixture.mine).has_value());
        const auto rejected = gitsync::push(fixture.mine, plain);
        QVERIFY(!rejected.has_value());
        QVERIFY2(gitsync::isNonFastForward(rejected.error().message), why(rejected).constData());
        const auto forced = gitsync::push(fixture.mine, lease);
        QVERIFY2(forced.has_value(), why(forced).constData());
        QCOMPARE(revParse(fixture.bare, QStringLiteral("main")), revParse(fixture.mine, QStringLiteral("HEAD")));
    }

    void upstreamCanBeSetAndUnset()
    {
        Fixture fixture;
        runGit(fixture.mine, { QStringLiteral("branch"), QStringLiteral("other") });
        QVERIFY(gitsync::setUpstream(fixture.mine, QStringLiteral("other"), QStringLiteral("origin/main")).has_value());
        QCOMPARE(gitsync::trackingOf(fixture.mine, QStringLiteral("other")).branch, QStringLiteral("main"));
        QVERIFY(gitsync::unsetUpstream(fixture.mine, QStringLiteral("other")).has_value());
        QVERIFY(gitsync::trackingOf(fixture.mine, QStringLiteral("other")).remote.isEmpty());
    }

    void pushesATag()
    {
        Fixture fixture;
        runGit(fixture.mine, { QStringLiteral("tag"), QStringLiteral("v1") });
        QVERIFY(gitsync::pushTag(fixture.mine, QStringLiteral("origin"), QStringLiteral("v1")).has_value());
        QCOMPARE(revParse(fixture.bare, QStringLiteral("v1")), revParse(fixture.mine, QStringLiteral("v1")));
    }

    void defaultRemoteIsTheOnlyOneOrOrigin()
    {
        QCOMPARE(gitsync::defaultRemote({ QStringLiteral("fork") }), QStringLiteral("fork"));
        QCOMPARE(gitsync::defaultRemote({ QStringLiteral("fork"), QStringLiteral("origin") }), QStringLiteral("origin"));
        QVERIFY(gitsync::defaultRemote({ QStringLiteral("fork"), QStringLiteral("upstream") }).isEmpty());
        QVERIFY(gitsync::defaultRemote({}).isEmpty());
    }

    void credentialsPointAtTheAskpassUnlessTheUserHasOne()
    {
        Fixture fixture;
        const auto options = gitsync::withCredentials(fixture.mine, {}, QStringLiteral("/usr/bin/gitnaga"));
        if (!qEnvironmentVariableIsEmpty("GIT_ASKPASS")) {
            QVERIFY(!options.environment.contains(QStringLiteral("GIT_ASKPASS")));
        } else {
            QCOMPARE(options.environment.value(QStringLiteral("GIT_ASKPASS")), QStringLiteral("/usr/bin/gitnaga"));
            QCOMPARE(options.environment.value(QStringLiteral("GITNAGA_ASKPASS")), QStringLiteral("1"));
        }
        runGit(fixture.mine, { QStringLiteral("config"), QStringLiteral("core.askPass"), QStringLiteral("/bin/mine") });
        QVERIFY(!gitsync::withCredentials(fixture.mine, {}, QStringLiteral("/usr/bin/gitnaga"))
                     .environment.contains(QStringLiteral("GIT_ASKPASS")));
    }
};

QTEST_GUILESS_MAIN(GitSyncTest)

#include "git_sync_test.moc"
