#include "git_test_helpers.hpp"
#include "remote_sync.hpp"
#include "repository_controller.hpp"

#include <QDir>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

// RemoteSync as the window drives it: through the controller's queue, with
// the signals QML turns into dialogs.

using namespace GitNaga;

namespace {

QString makeClone(const QString &root, const QString &bare, const QString &name)
{
    const auto path = QDir(root).filePath(name);
    runGit(root, { QStringLiteral("clone"), QStringLiteral("-q"), bare, path });
    runGit(path, { QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("GitNaga Test") });
    runGit(path, { QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@gitnaga.invalid") });
    return path;
}

void commit(const QString &repository, const QString &file)
{
    writeFile(QDir(repository).filePath(file), file.toUtf8() + '\n');
    runGit(repository, { QStringLiteral("add"), file });
    runGit(repository, { QStringLiteral("commit"), QStringLiteral("-qm"), file });
}

struct Remotes {
    QTemporaryDir directory;
    QString bare;
    QString mine;
    QString theirs;

    Remotes()
    {
        bare = QDir(directory.path()).filePath(QStringLiteral("remote.git"));
        runGit(directory.path(), { QStringLiteral("init"), QStringLiteral("-q"), QStringLiteral("--bare"),
                                   QStringLiteral("-b"), QStringLiteral("main"), bare });
        const auto seed = makeClone(directory.path(), bare, QStringLiteral("seed"));
        commit(seed, QStringLiteral("seed.txt"));
        runGit(seed, { QStringLiteral("push"), QStringLiteral("-q"), QStringLiteral("origin"), QStringLiteral("HEAD:main") });
        mine = makeClone(directory.path(), bare, QStringLiteral("mine"));
        theirs = makeClone(directory.path(), bare, QStringLiteral("theirs"));
    }

    void theyPush()
    {
        commit(theirs, QStringLiteral("theirs.txt"));
        runGit(theirs, { QStringLiteral("push"), QStringLiteral("-q"), QStringLiteral("origin"), QStringLiteral("main") });
    }
};

void open(RepositoryController &controller, const QString &path)
{
    controller.openRepositoryPath(path);
    QTRY_COMPARE_WITH_TIMEOUT(controller.currentBranch(), QStringLiteral("main"), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.historyLoading() && !controller.loading(), 5000);
}

void settle(RepositoryController &controller)
{
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 10000);
    QTest::qWait(300);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.loading(), 5000);
}

} // namespace

class SyncControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void rejectedPushOffersALease()
    {
        Remotes remotes;
        remotes.theyPush();
        commit(remotes.mine, QStringLiteral("mine.txt"));
        RepositoryController controller;
        open(controller, remotes.mine);
        controller.sync()->fetch();
        settle(controller);
        QCOMPARE(controller.behind(), 1);

        QSignalSpy rejected(controller.sync(), &RemoteSync::pushRejected);
        controller.sync()->push();
        settle(controller);
        QCOMPARE(rejected.count(), 1);
        QCOMPARE(rejected.at(0).at(0).toString(), QStringLiteral("main"));
        QCOMPARE(rejected.at(0).at(1).toString(), QStringLiteral("origin"));
        QVERIFY(!controller.lastOperationOk());

        controller.sync()->forcePushWithLease(QStringLiteral("main"), QStringLiteral("origin"));
        settle(controller);
        QVERIFY2(controller.lastOperationOk(), qPrintable(controller.operationMessage()));
        QCOMPARE(controller.ahead(), 0);
        QCOMPARE(controller.behind(), 0);
    }

    void pullModeIsRememberedAndPullLands()
    {
        Remotes remotes;
        remotes.theyPush();
        RepositoryController controller;
        open(controller, remotes.mine);
        controller.sync()->fetch();
        settle(controller);
        controller.sync()->pullWith(QStringLiteral("ff-only"));
        settle(controller);
        QVERIFY2(controller.lastOperationOk(), qPrintable(controller.operationMessage()));
        QCOMPARE(controller.behind(), 0);
        QCOMPARE(QSettings().value(QStringLiteral("sync/pullMode")).toString(), QStringLiteral("ff-only"));
        RemoteSync fresh(&controller);
        QCOMPARE(fresh.pullMode(), QStringLiteral("ff-only"));
    }

    void severalRemotesAskWhereToPublish()
    {
        Remotes remotes;
        runGit(remotes.mine, { QStringLiteral("remote"), QStringLiteral("rename"), QStringLiteral("origin"), QStringLiteral("fork") });
        runGit(remotes.mine, { QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("upstream"), remotes.bare });
        runGit(remotes.mine, { QStringLiteral("switch"), QStringLiteral("-qc"), QStringLiteral("topic") });
        RepositoryController controller;
        controller.openRepositoryPath(remotes.mine);
        QTRY_COMPARE_WITH_TIMEOUT(controller.currentBranch(), QStringLiteral("topic"), 5000);

        QSignalSpy choice(controller.sync(), &RemoteSync::remoteChoiceNeeded);
        controller.sync()->push();
        settle(controller);
        QCOMPARE(choice.count(), 1);
        QCOMPARE(choice.at(0).at(1).toStringList(), QStringList({ QStringLiteral("fork"), QStringLiteral("upstream") }));

        controller.sync()->pushTo(QStringLiteral("upstream"));
        settle(controller);
        QCOMPARE(controller.upstream(), QStringLiteral("upstream/topic"));
    }

    void autoFetchRunsQuietlyWhileActive()
    {
        Remotes remotes;
        RepositoryController controller;
        open(controller, remotes.mine);
        bool active = false;
        controller.sync()->setActivityProbe([&active] { return active; });
        controller.sync()->setAutoFetch(true);
        controller.sync()->setAutoFetchInterval(400);
        remotes.theyPush();

        // An inactive window does not reach the network.
        QTest::qWait(1200);
        QCOMPARE(controller.behind(), 0);
        active = true;
        QTRY_COMPARE_WITH_TIMEOUT(controller.behind(), 1, 5000);
        // Quiet: no success toast for a background fetch.
        QVERIFY(controller.operationMessage().isEmpty());

        controller.sync()->setAutoFetch(false);
        QCOMPARE(QSettings().value(QStringLiteral("sync/autoFetch")).toBool(), false);
    }

    void pullWithoutUpstreamSaysWhy()
    {
        Remotes remotes;
        runGit(remotes.mine, { QStringLiteral("switch"), QStringLiteral("-qc"), QStringLiteral("local-only") });
        RepositoryController controller;
        controller.openRepositoryPath(remotes.mine);
        QTRY_COMPARE_WITH_TIMEOUT(controller.currentBranch(), QStringLiteral("local-only"), 5000);
        controller.sync()->pull();
        QVERIFY(!controller.lastOperationOk());
        QVERIFY2(controller.operationMessage().contains(QStringLiteral("no upstream")), qPrintable(controller.operationMessage()));
    }
};

QTEST_MAIN(SyncControllerTest)

#include "sync_controller_test.moc"
