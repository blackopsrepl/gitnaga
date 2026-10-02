#include "git_client.hpp"
#include "git_test_helpers.hpp"
#include "repository_controller.hpp"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTest>

using namespace GitNaga;

namespace {

QString initializeRepository(const QString &path)
{
    QDir().mkpath(path);
    runGit(path, { QStringLiteral("init"), QStringLiteral("-b"), QStringLiteral("main") });
    runGit(path, { QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("GitNaga Test") });
    runGit(path, { QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@gitnaga.invalid") });
    writeFile(path + QStringLiteral("/base.txt"), QByteArrayLiteral("base\n"));
    runGit(path, { QStringLiteral("add"), QStringLiteral("base.txt") });
    runGit(path, { QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("base") });
    return path;
}

const Worktree *findWorktree(const QVector<Worktree> &worktrees, const QString &path)
{
    for (const auto &worktree : worktrees) {
        if (worktree.path == path)
            return &worktree;
    }
    return nullptr;
}

} // namespace

class WorktreeTest final : public QObject
{
    Q_OBJECT

private slots:
    void listsMainLinkedAndDetachedWorktrees()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = initializeRepository(directory.path() + QStringLiteral("/main repo"));
        runGit(root, { QStringLiteral("branch"), QStringLiteral("existing") });
        const auto linkedPath = directory.path() + QStringLiteral("/feature tree");
        const auto added = GitClient::addWorktree(root, linkedPath, QStringLiteral("feature"), true);
        if (!added)
            qWarning() << added.error().message;
        QVERIFY(added.has_value());
        const auto existingPath = directory.path() + QStringLiteral("/existing tree");
        QVERIFY(GitClient::addWorktree(root, existingPath, QStringLiteral("existing")).has_value());

        const auto detachedPath = directory.path() + QStringLiteral("/detached tree");
        const auto detached = GitClient::addWorktree(root, detachedPath, {}, false, true);
        if (!detached)
            qWarning() << detached.error().message;
        QVERIFY(detached.has_value());

        const auto worktrees = GitClient::listWorktrees(root);
        if (!worktrees)
            qWarning() << worktrees.error().message;
        QVERIFY(worktrees.has_value());
        QCOMPARE(worktrees->size(), 4);
        const auto *mainWorktree = findWorktree(*worktrees, root);
        const auto *linked = findWorktree(*worktrees, linkedPath);
        const auto *existing = findWorktree(*worktrees, existingPath);
        const auto *detachedEntry = findWorktree(*worktrees, detachedPath);
        QVERIFY(mainWorktree);
        QVERIFY(mainWorktree->isMain);
        QCOMPARE(mainWorktree->branch, QStringLiteral("main"));
        QVERIFY(!mainWorktree->isDetached);
        QVERIFY(linked);
        QVERIFY(!linked->isMain);
        QCOMPARE(linked->branch, QStringLiteral("feature"));
        QVERIFY(!linked->isDetached);
        QVERIFY(existing);
        QCOMPARE(existing->branch, QStringLiteral("existing"));
        QVERIFY(!existing->isDetached);
        QVERIFY(detachedEntry);
        QVERIFY(detachedEntry->branch.isEmpty());
        QVERIFY(detachedEntry->isDetached);
    }

    void removalProtectsDirtyWorktreesUnlessForced()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = initializeRepository(directory.path() + QStringLiteral("/main"));
        const auto linkedPath = directory.path() + QStringLiteral("/linked");
        const auto added = GitClient::addWorktree(root, linkedPath, QStringLiteral("topic"), true);
        QVERIFY(added.has_value());
        writeFile(linkedPath + QStringLiteral("/untracked.txt"), QByteArrayLiteral("keep me\n"));

        const auto refused = GitClient::removeWorktree(root, linkedPath);
        QVERIFY(!refused.has_value());
        QVERIFY(QFile::exists(linkedPath + QStringLiteral("/untracked.txt")));
        auto worktrees = GitClient::listWorktrees(root);
        QVERIFY(worktrees.has_value());
        QCOMPARE(worktrees->size(), 2);

        const auto removed = GitClient::removeWorktree(root, linkedPath, true);
        QVERIFY(removed.has_value());
        QVERIFY(!QFile::exists(linkedPath));
        worktrees = GitClient::listWorktrees(root);
        QVERIFY(worktrees.has_value());
        QCOMPARE(worktrees->size(), 1);
    }

    void opensAndAddsWorktreesFromBareRepositories()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto source = initializeRepository(directory.path() + QStringLiteral("/source"));
        const auto bare = directory.path() + QStringLiteral("/bare.git");
        runGit(directory.path(), { QStringLiteral("clone"), QStringLiteral("--bare"), source, bare });

        const auto snapshot = GitClient::loadRepository(bare);
        QVERIFY(snapshot.has_value());
        QCOMPARE(snapshot->worktrees.size(), 1);
        QVERIFY(snapshot->worktrees.first().isMain);
        QVERIFY(snapshot->worktrees.first().isBare);

        const auto linkedPath = directory.path() + QStringLiteral("/bare checkout");
        const auto added = GitClient::addWorktree(bare, linkedPath, QStringLiteral("bare-branch"), true);
        QVERIFY(added.has_value());
        const auto worktrees = GitClient::listWorktrees(bare);
        QVERIFY(worktrees.has_value());
        QCOMPARE(worktrees->size(), 2);
        const auto *linked = findWorktree(*worktrees, linkedPath);
        QVERIFY(linked);
        QCOMPARE(linked->branch, QStringLiteral("bare-branch"));
        QVERIFY(!linked->isBare);
    }

    void snapshotsLinkedWorktreeWithoutTouchingTheMainIndex()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = initializeRepository(directory.path() + QStringLiteral("/main"));
        const auto linkedPath = directory.path() + QStringLiteral("/linked");
        QVERIFY(GitClient::addWorktree(root, linkedPath, QStringLiteral("topic"), true).has_value());
        writeFile(linkedPath + QStringLiteral("/base.txt"), QByteArrayLiteral("staged\n"));
        runGit(linkedPath, { QStringLiteral("add"), QStringLiteral("base.txt") });
        writeFile(linkedPath + QStringLiteral("/base.txt"), QByteArrayLiteral("unstaged\n"));
        writeFile(linkedPath + QStringLiteral("/new.txt"), QByteArrayLiteral("new\n"));

        const auto snapshot = GitClient::loadRepository(linkedPath);
        QVERIFY(snapshot.has_value());
        QCOMPARE(snapshot->worktree, linkedPath);
        QCOMPARE(snapshot->commonDirectory, root + QStringLiteral("/.git"));
        QVERIFY(snapshot->gitDirectory != snapshot->commonDirectory);
        QVERIFY(!snapshot->workInProgressOid.isEmpty());
        const auto changes = GitClient::inspectWorktree(linkedPath);
        QVERIFY(changes.has_value());
        QCOMPARE(changes->files.size(), 2);
        const auto mainStatus = GitClient::mutate(root, { QStringLiteral("status"), QStringLiteral("--porcelain") },
                                                  QStringLiteral("read main status"));
        QVERIFY(mainStatus.has_value());
        QVERIFY(mainStatus->trimmed().isEmpty());
    }

    void controllerSwitchesWorktreesAndGuardsActiveRemoval()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = initializeRepository(directory.path() + QStringLiteral("/main"));
        const auto linkedPath = directory.path() + QStringLiteral("/linked");
        RepositoryController controller;
        QSignalSpy operations(&controller, &RepositoryController::operationFinished);
        controller.openRepositoryPath(root);
        QTRY_COMPARE_WITH_TIMEOUT(controller.repositoryPath(), root, 5000);
        QCOMPARE(controller.worktrees().size(), 1);

        controller.addWorktree(linkedPath, QStringLiteral("topic"), QStringLiteral("new"), {});
        QTRY_COMPARE_WITH_TIMEOUT(controller.worktrees().size(), 2, 5000);
        QCOMPARE(operations.count(), 1);
        QVERIFY(operations.takeFirst().at(0).toBool());
        controller.openWorktree(linkedPath);
        QTRY_COMPARE_WITH_TIMEOUT(controller.repositoryPath(), linkedPath, 5000);
        controller.removeWorktree(linkedPath, true);
        QCOMPARE(operations.count(), 1);
        QVERIFY(!operations.takeFirst().at(0).toBool());
        QVERIFY(QFile::exists(linkedPath + QStringLiteral("/base.txt")));

        controller.openWorktree(root);
        QTRY_COMPARE_WITH_TIMEOUT(controller.repositoryPath(), root, 5000);
        controller.removeWorktree(linkedPath, false);
        QTRY_COMPARE_WITH_TIMEOUT(controller.worktrees().size(), 1, 5000);
        QVERIFY(!QDir(linkedPath).exists());
    }

    void controllerRejectsPrunableWorktrees()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = initializeRepository(directory.path() + QStringLiteral("/main"));
        const auto linkedPath = directory.path() + QStringLiteral("/linked");
        RepositoryController controller;
        QSignalSpy operations(&controller, &RepositoryController::operationFinished);
        controller.openRepositoryPath(root);
        QTRY_COMPARE_WITH_TIMEOUT(controller.repositoryPath(), root, 5000);
        controller.addWorktree(linkedPath, QStringLiteral("topic"), QStringLiteral("new"), {});
        QTRY_COMPARE_WITH_TIMEOUT(controller.worktrees().size(), 2, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(operations.count(), 1, 5000);
        operations.clear();

        QVERIFY(QDir(linkedPath).removeRecursively());
        runGit(root, { QStringLiteral("config"), QStringLiteral("gc.worktreePruneExpire"), QStringLiteral("now") });
        controller.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(controller.worktrees().at(1).toMap().value(QStringLiteral("isPrunable")).toBool(), 5000);
        controller.openWorktree(linkedPath);
        QCOMPARE(operations.count(), 1);
        QVERIFY(!operations.takeFirst().at(0).toBool());
        QCOMPARE(controller.repositoryPath(), root);
        controller.removeWorktree(linkedPath, true);
        QCOMPARE(operations.count(), 1);
        QVERIFY(!operations.takeFirst().at(0).toBool());
    }

    void locksMovesAndPrunesWorktrees()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = initializeRepository(directory.path() + QStringLiteral("/main"));
        const auto linkedPath = directory.path() + QStringLiteral("/linked");
        const auto movedPath = directory.path() + QStringLiteral("/moved");
        const auto added = GitClient::addWorktree(root, linkedPath, QStringLiteral("topic"), true);
        QVERIFY(added.has_value());

        const auto locked = GitClient::lockWorktree(root, linkedPath, QStringLiteral("in use"));
        QVERIFY(locked.has_value());
        auto worktrees = GitClient::listWorktrees(root);
        QVERIFY(worktrees.has_value());
        QVERIFY(worktrees->at(1).isLocked);
        QCOMPARE(worktrees->at(1).lockReason, QStringLiteral("in use"));
        QVERIFY(GitClient::unlockWorktree(root, linkedPath).has_value());
        QVERIFY(GitClient::moveWorktree(root, linkedPath, movedPath).has_value());
        worktrees = GitClient::listWorktrees(root);
        QVERIFY(worktrees.has_value());
        QCOMPARE(worktrees->at(1).path, movedPath);
        QVERIFY(!worktrees->at(1).isLocked);
        const auto repaired = GitClient::repairWorktrees(root);
        QVERIFY(repaired.has_value());

        QVERIFY(QDir(movedPath).removeRecursively());
        worktrees = GitClient::listWorktrees(root);
        QVERIFY(worktrees.has_value());
        const auto *stale = findWorktree(*worktrees, movedPath);
        QVERIFY(stale);
        QVERIFY(stale->isPrunable);
        runGit(root, { QStringLiteral("config"), QStringLiteral("gc.worktreePruneExpire"), QStringLiteral("now") });
        QVERIFY(GitClient::pruneWorktrees(root).has_value());
        QCOMPARE(GitClient::listWorktrees(root)->size(), 1);
    }
};

QTEST_GUILESS_MAIN(WorktreeTest)

#include "worktree_test.moc"
