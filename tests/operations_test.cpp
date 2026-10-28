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

// Half-done operations driven through the controller against real
// repositories: the flows a person reaches from the banner.
class OperationsTest final : public QObject
{
    Q_OBJECT

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

        controller.operation()->resolveConflict(QStringLiteral("f.txt"), QStringLiteral("theirs"));
        QTRY_COMPARE_WITH_TIMEOUT(controller.inProgress()->conflictCount(), 0, 5000);
        settle(controller);
        controller.operation()->continueOperation();
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

        const auto document = controller.operation()->conflictDocument(QStringLiteral("f.txt"));
        QCOMPARE(document.value(QStringLiteral("conflicts")).toInt(), 1);
        controller.operation()->saveConflictResolution(QStringLiteral("f.txt"), { QStringLiteral("ours-theirs") });
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

        controller.operation()->abortOperation();
        QTRY_COMPARE_WITH_TIMEOUT(controller.operationInProgress(), QString(), 5000);
        settle(controller);
        QCOMPARE(controller.headOid(), before);
        QCOMPARE(controller.currentBranch(), QStringLiteral("other"));
        QCOMPARE(contents(root), QStringLiteral("theirs\n"));
    }

    void mergetoolResolvesTheFile()
    {
        QTemporaryDir directory;
        const auto root = directory.path();
        makeDivergent(root);
        runGit(root, { QStringLiteral("config"), QStringLiteral("merge.tool"), QStringLiteral("takeTheirs") });
        runGit(root, { QStringLiteral("config"), QStringLiteral("mergetool.takeTheirs.cmd"),
                       QStringLiteral("cp \"$REMOTE\" \"$MERGED\"") });
        runGit(root, { QStringLiteral("config"), QStringLiteral("mergetool.takeTheirs.trustExitCode"), QStringLiteral("true") });
        runGit(root, { QStringLiteral("config"), QStringLiteral("mergetool.keepBackup"), QStringLiteral("false") });
        RepositoryController controller;
        open(controller, root);
        controller.mergeCommit(git(root, { QStringLiteral("rev-parse"), QStringLiteral("other") }), QStringLiteral("other"));
        QTRY_COMPARE_WITH_TIMEOUT(controller.inProgress()->conflictCount(), 1, 5000);
        settle(controller);

        controller.operation()->openMergetool(QStringLiteral("f.txt"));
        QTRY_COMPARE_WITH_TIMEOUT(controller.inProgress()->conflictCount(), 0, 10000);
        settle(controller);
        QVERIFY2(controller.lastOperationOk(), qPrintable(controller.operationMessage()));
        QCOMPARE(contents(root), QStringLiteral("theirs\n"));
    }

    // The editor is whatever opens the file on this desktop; when nothing
    // does, the footer says so instead of the click doing nothing.
    void openInEditorHandsTheFileToTheDesktop()
    {
        QTemporaryDir directory;
        const auto root = directory.path();
        makeDivergent(root);
        const auto bin = root + QStringLiteral("/bin");
        QDir().mkpath(bin);
        writeFile(bin + QStringLiteral("/xdg-open"), "#!/bin/sh\nprintf '%s' \"$1\" > \"$(dirname \"$0\")/opened\"\n");
        QFile::setPermissions(bin + QStringLiteral("/xdg-open"), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        const auto path = qgetenv("PATH");
        qputenv("PATH", bin.toUtf8() + ':' + path);
        RepositoryController controller;
        open(controller, root);
        controller.operation()->openInEditor(QStringLiteral("f.txt"));
        qputenv("PATH", path);
        QFile opened(bin + QStringLiteral("/opened"));
        QTRY_VERIFY_WITH_TIMEOUT(opened.exists() || !controller.operationMessage().isEmpty(), 5000);
        if (opened.exists()) {
            QVERIFY(opened.open(QIODevice::ReadOnly));
            QVERIFY2(opened.readAll().endsWith("/f.txt"), "xdg-open got another file");
            qInfo("handed to xdg-open");
        } else {
            QVERIFY(!controller.lastOperationOk());
            QCOMPARE(controller.operationMessage(), QStringLiteral("No application is set to open f.txt"));
            qInfo("no desktop services: reported");
        }
    }
};

QTEST_MAIN(OperationsTest)
#include "operations_test.moc"
