#include "git_client.hpp"
#include "operation_controller.hpp"
#include "operation_fixtures.hpp"
#include "undo_stack.hpp"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace GitNaga;
using namespace OperationFixtures;

// Undo against real repositories: what Ctrl+Z takes back, and when it
// refuses to because the refs moved some other way.
class UndoTest final : public QObject
{
    Q_OBJECT

private slots:
    void undoCommitReturnsChangesStaged()
    {
        QTemporaryDir directory;
        const auto root = directory.path();
        makeDivergent(root);
        const auto before = git(root, { QStringLiteral("rev-parse"), QStringLiteral("HEAD") });
        writeFile(root + QStringLiteral("/f.txt"), "edited\n");
        runGit(root, { QStringLiteral("add"), QStringLiteral("f.txt") });
        RepositoryController controller;
        open(controller, root);
        controller.selectOid(worktreeOid());
        controller.commitWorktree(QStringLiteral("edit"), {});
        QTRY_VERIFY_WITH_TIMEOUT(controller.headOid() != before, 5000);
        settle(controller);
        QCOMPARE(controller.undoStack()->label(), QStringLiteral("Undo commit"));

        controller.undo();
        QTRY_COMPARE_WITH_TIMEOUT(controller.headOid(), before, 5000);
        settle(controller);
        QCOMPARE(git(root, { QStringLiteral("diff"), QStringLiteral("--cached"), QStringLiteral("--name-only") }),
                 QStringLiteral("f.txt"));
        QVERIFY(!controller.undoStack()->available());
    }

    void undoBranchDeleteAndCheckout()
    {
        QTemporaryDir directory;
        const auto root = directory.path();
        makeDivergent(root);
        const auto other = git(root, { QStringLiteral("rev-parse"), QStringLiteral("other") });
        runGit(root, { QStringLiteral("branch"), QStringLiteral("spare"), QStringLiteral("other") });
        RepositoryController controller;
        open(controller, root);

        controller.checkoutBranch(QStringLiteral("other"));
        QTRY_COMPARE_WITH_TIMEOUT(controller.currentBranch(), QStringLiteral("other"), 5000);
        settle(controller);
        controller.deleteBranch(QStringLiteral("spare"));
        QTRY_COMPARE_WITH_TIMEOUT(git(root, { QStringLiteral("branch"), QStringLiteral("--list"), QStringLiteral("spare") }),
                                  QString(), 5000);
        settle(controller);
        QCOMPARE(controller.undoStack()->label(), QStringLiteral("Undo delete branch spare"));

        controller.undo();
        QTRY_COMPARE_WITH_TIMEOUT(git(root, { QStringLiteral("rev-parse"), QStringLiteral("refs/heads/spare") }), other, 5000);
        settle(controller);
        // The stack holds the checkout below the deletion.
        QCOMPARE(controller.undoStack()->label(), QStringLiteral("Undo checkout"));
        controller.undo();
        QTRY_COMPARE_WITH_TIMEOUT(controller.currentBranch(), QStringLiteral("main"), 5000);
    }

    void undoAnnotatedTagDeleteKeepsTagObject()
    {
        QTemporaryDir directory;
        const auto root = directory.path();
        makeDivergent(root);
        runGit(root, { QStringLiteral("tag"), QStringLiteral("-a"), QStringLiteral("v1"), QStringLiteral("-m"),
                       QStringLiteral("release") });
        const auto tagObject = git(root, { QStringLiteral("rev-parse"), QStringLiteral("refs/tags/v1") });
        RepositoryController controller;
        open(controller, root);

        controller.deleteTag(QStringLiteral("v1"));
        QTRY_COMPARE_WITH_TIMEOUT(git(root, { QStringLiteral("tag"), QStringLiteral("--list"), QStringLiteral("v1") }),
                                  QString(), 5000);
        settle(controller);
        controller.undo();
        QTRY_COMPARE_WITH_TIMEOUT(git(root, { QStringLiteral("rev-parse"), QStringLiteral("refs/tags/v1") }), tagObject, 5000);
        QCOMPARE(git(root, { QStringLiteral("cat-file"), QStringLiteral("-t"), QStringLiteral("v1") }), QStringLiteral("tag"));
    }

    void externalRefChangeClearsUndo()
    {
        QTemporaryDir directory;
        const auto root = directory.path();
        makeDivergent(root);
        RepositoryController controller;
        open(controller, root);
        controller.createTag(QStringLiteral("mine"), controller.headOid());
        QTRY_VERIFY_WITH_TIMEOUT(controller.undoStack()->available(), 5000);
        settle(controller);

        // Someone else moved the refs: the recorded inverse no longer applies.
        runGit(root, { QStringLiteral("branch"), QStringLiteral("elsewhere") });
        controller.synchronize();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.undoStack()->available(), 5000);
    }

    // A cherry-pick of several commits stopped on the middle one, from a
    // terminal; continuing it in GitNaga is undone back past all of them.
    void cherryPickSequenceUndoesAsAWhole()
    {
        QTemporaryDir directory;
        const auto root = directory.path();
        makeDivergent(root);
        runGit(root, { QStringLiteral("switch"), QStringLiteral("-q"), QStringLiteral("other") });
        writeFile(root + QStringLiteral("/a.txt"), "a\n");
        runGit(root, { QStringLiteral("add"), QStringLiteral("a.txt") });
        runGit(root, { QStringLiteral("commit"), QStringLiteral("-qm"), QStringLiteral("add a") });
        writeFile(root + QStringLiteral("/b.txt"), "b\n");
        runGit(root, { QStringLiteral("add"), QStringLiteral("b.txt") });
        runGit(root, { QStringLiteral("commit"), QStringLiteral("-qm"), QStringLiteral("add b") });
        runGit(root, { QStringLiteral("switch"), QStringLiteral("-q"), QStringLiteral("main") });
        const auto before = git(root, { QStringLiteral("rev-parse"), QStringLiteral("HEAD") });
        // "theirs" conflicts with main's "ours"; "add a" lands first, "add b" waits.
        git(root, { QStringLiteral("cherry-pick"), QStringLiteral("other~1"), QStringLiteral("other~2"), QStringLiteral("other") });
        RepositoryController controller;
        open(controller, root);
        QTRY_COMPARE_WITH_TIMEOUT(controller.operationInProgress(), QStringLiteral("cherry-pick"), 5000);
        QVERIFY(git(root, { QStringLiteral("rev-parse"), QStringLiteral("HEAD") }) != before);

        controller.operation()->resolveConflict(QStringLiteral("f.txt"), QStringLiteral("theirs"));
        QTRY_COMPARE_WITH_TIMEOUT(controller.inProgress()->conflictCount(), 0, 5000);
        settle(controller);
        controller.operation()->continueOperation();
        QTRY_COMPARE_WITH_TIMEOUT(controller.operationInProgress(), QString(), 5000);
        settle(controller);
        QCOMPARE(git(root, { QStringLiteral("rev-list"), QStringLiteral("--count"), before + QStringLiteral("..HEAD") }),
                 QStringLiteral("3"));
        QCOMPARE(controller.undoStack()->label(), QStringLiteral("Undo cherry-pick"));

        controller.undo();
        QTRY_COMPARE_WITH_TIMEOUT(controller.headOid(), before, 5000);
        QCOMPARE(contents(root), QStringLiteral("ours\n"));
    }

    // The configured merge tool runs through the queue; its result lands in
    // the index and the footer.
    void remoteDeletionIsNotUndoable()
    {
        UndoStack stack;
        stack.observe(QStringLiteral("/r"), "a");
        stack.block(QStringLiteral("delete remote branch"), QStringLiteral("/r"));
        stack.observe(QStringLiteral("/r"), "b");
        QVERIFY(!stack.available());
        QCOMPARE(stack.label(), QStringLiteral("Can't undo delete remote branch"));
        stack.observe(QStringLiteral("/r"), "c");
        QCOMPARE(stack.label(), QString());
    }
};

QTEST_MAIN(UndoTest)
#include "undo_test.moc"
