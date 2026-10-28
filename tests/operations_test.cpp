#include "git_client.hpp"
#include "git_test_helpers.hpp"
#include "repository_controller.hpp"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace GitNaga;

// Half-done operations and undo, driven through the controller against real
// repositories: the flows a person reaches from the banner and Ctrl+Z.
class OperationsTest final : public QObject
{
    Q_OBJECT

    static QString git(const QString &root, const QStringList &arguments)
    {
        QProcess process;
        process.setWorkingDirectory(root);
        process.start(QStringLiteral("git"), arguments);
        process.waitForFinished();
        return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    }

    static void commitFile(const QString &root, const QByteArray &contents, const QString &message)
    {
        writeFile(root + QStringLiteral("/f.txt"), contents);
        runGit(root, { QStringLiteral("add"), QStringLiteral("f.txt") });
        runGit(root, { QStringLiteral("commit"), QStringLiteral("-qm"), message });
    }

    // main and other both rewrote f.txt; main is checked out.
    static void makeDivergent(const QString &root)
    {
        runGit(root, { QStringLiteral("init"), QStringLiteral("-q"), QStringLiteral("-b"), QStringLiteral("main") });
        runGit(root, { QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("GitNaga Test") });
        runGit(root, { QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@gitnaga.invalid") });
        commitFile(root, "base\n", QStringLiteral("base"));
        runGit(root, { QStringLiteral("switch"), QStringLiteral("-qc"), QStringLiteral("other") });
        commitFile(root, "theirs\n", QStringLiteral("theirs"));
        runGit(root, { QStringLiteral("switch"), QStringLiteral("-q"), QStringLiteral("main") });
        commitFile(root, "ours\n", QStringLiteral("ours"));
    }

    static void open(RepositoryController &controller, const QString &root)
    {
        controller.openRepositoryPath(root);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.headOid().isEmpty(), 5000);
        settle(controller);
    }

    static void settle(RepositoryController &controller)
    {
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy() && !controller.loading() && !controller.historyLoading(), 10000);
        QTest::qWait(200);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy() && !controller.loading(), 10000);
    }

    static QString contents(const QString &root)
    {
        QFile file(root + QStringLiteral("/f.txt"));
        return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
    }

private slots:
    void mergeConflictTakesTheirsAndCommits()
    {
        QTemporaryDir directory;
        const auto root = directory.path();
        makeDivergent(root);
        RepositoryController controller;
        open(controller, root);

        controller.mergeCommit(git(root, { QStringLiteral("rev-parse"), QStringLiteral("other") }), QStringLiteral("other"));
        QTRY_COMPARE_WITH_TIMEOUT(controller.operationInProgress(), QStringLiteral("merge"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(controller.inProgress()->conflictCount(), 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(controller.inProgress()->title(), QStringLiteral("Merging other into main"), 5000);
        QVERIFY(!controller.lastOperationOk());

        controller.resolveConflict(QStringLiteral("f.txt"), QStringLiteral("theirs"));
        QTRY_COMPARE_WITH_TIMEOUT(controller.inProgress()->conflictCount(), 0, 5000);
        settle(controller);
        controller.continueOperation();
        QTRY_COMPARE_WITH_TIMEOUT(controller.operationInProgress(), QString(), 5000);
        settle(controller);

        const auto parents = git(root, { QStringLiteral("rev-list"), QStringLiteral("--parents"), QStringLiteral("-1"),
                                         QStringLiteral("HEAD") }).split(QLatin1Char(' '));
        QCOMPARE(parents.size(), 3);
        QCOMPARE(contents(root), QStringLiteral("theirs\n"));
        QCOMPARE(controller.undoStack()->label(), QStringLiteral("Undo merge"));
    }

    void conflictViewSavesBothSides()
    {
        QTemporaryDir directory;
        const auto root = directory.path();
        makeDivergent(root);
        RepositoryController controller;
        open(controller, root);
        controller.mergeCommit(git(root, { QStringLiteral("rev-parse"), QStringLiteral("other") }), QStringLiteral("other"));
        QTRY_COMPARE_WITH_TIMEOUT(controller.inProgress()->conflictCount(), 1, 5000);

        const auto document = controller.conflictDocument(QStringLiteral("f.txt"));
        QCOMPARE(document.value(QStringLiteral("conflicts")).toInt(), 1);
        controller.saveConflictResolution(QStringLiteral("f.txt"), { QStringLiteral("ours-theirs") });
        QTRY_COMPARE_WITH_TIMEOUT(controller.inProgress()->conflictCount(), 0, 5000);
        QCOMPARE(contents(root), QStringLiteral("ours\ntheirs\n"));
        // Staged: the index has no unmerged entries left.
        QCOMPARE(git(root, { QStringLiteral("diff"), QStringLiteral("--name-only"), QStringLiteral("--diff-filter=U") }),
                 QString());
    }

    void rebaseConflictAbortRestores()
    {
        QTemporaryDir directory;
        const auto root = directory.path();
        makeDivergent(root);
        runGit(root, { QStringLiteral("switch"), QStringLiteral("-q"), QStringLiteral("other") });
        const auto before = git(root, { QStringLiteral("rev-parse"), QStringLiteral("HEAD") });
        RepositoryController controller;
        open(controller, root);

        controller.rebaseOnto(git(root, { QStringLiteral("rev-parse"), QStringLiteral("main") }), QStringLiteral("main"));
        QTRY_COMPARE_WITH_TIMEOUT(controller.operationInProgress(), QStringLiteral("rebase"), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(controller.inProgress()->title(), QStringLiteral("Rebasing other onto main — 1/1"), 5000);
        QVERIFY(controller.inProgress()->detail().contains(QStringLiteral("theirs")));
        QVERIFY(controller.inProgress()->canSkip());
        settle(controller);

        controller.abortOperation();
        QTRY_COMPARE_WITH_TIMEOUT(controller.operationInProgress(), QString(), 5000);
        settle(controller);
        QCOMPARE(controller.headOid(), before);
        QCOMPARE(controller.currentBranch(), QStringLiteral("other"));
        QCOMPARE(contents(root), QStringLiteral("theirs\n"));
    }

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

QTEST_MAIN(OperationsTest)
#include "operations_test.moc"
