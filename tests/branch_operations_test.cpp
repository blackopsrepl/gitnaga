#include "git_client.hpp"
#include "git_test_helpers.hpp"
#include "history_editor.hpp"
#include "repository_controller.hpp"

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace GitNaga;

// Branch drag-and-drop, the rebase editor's controller calls and hide/solo,
// driven the way the window drives them.
class BranchOperationsTest final : public QObject
{
    Q_OBJECT

    QTemporaryDir m_directory;
    QString m_root;

    QString git(const QStringList &arguments) const
    {
        auto output = GitClient::mutate(m_root, arguments, QStringLiteral("test"));
        return output ? *output : QString();
    }

    void commit(const QString &name)
    {
        writeFile(m_root + QLatin1Char('/') + name + QStringLiteral(".txt"), name.toUtf8() + '\n');
        runGit(m_root, { QStringLiteral("add"), QStringLiteral(".") });
        runGit(m_root, { QStringLiteral("commit"), QStringLiteral("-qm"), name });
    }

    static void settle(RepositoryController &controller)
    {
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy() && !controller.loading() && !controller.historyLoading(), 10000);
    }

    static bool wearsChip(RepositoryController &controller, const QString &name)
    {
        for (const auto &commit : controller.commits()->commits()) {
            for (const auto &ref : commit.refs) {
                if (ref.name == name)
                    return true;
            }
        }
        return false;
    }

private slots:
    // Hide/solo persist through QSettings; keep them out of the user's file.
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void init()
    {
        QVERIFY(m_directory.isValid());
        m_root = m_directory.path() + QStringLiteral("/repo");
        QDir(m_root).removeRecursively();
        QDir().mkpath(m_root);
        runGit(m_root, { QStringLiteral("init"), QStringLiteral("-q"), QStringLiteral("-b"), QStringLiteral("main") });
        runGit(m_root, { QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("GitNaga Test") });
        runGit(m_root, { QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@gitnaga.invalid") });
        runGit(m_root, { QStringLiteral("config"), QStringLiteral("commit.gpgsign"), QStringLiteral("false") });
        commit(QStringLiteral("base"));
        runGit(m_root, { QStringLiteral("branch"), QStringLiteral("topic") });
        commit(QStringLiteral("main-one"));
        commit(QStringLiteral("main-two"));
        runGit(m_root, { QStringLiteral("switch"), QStringLiteral("-q"), QStringLiteral("topic") });
        commit(QStringLiteral("topic-one"));
        runGit(m_root, { QStringLiteral("switch"), QStringLiteral("-q"), QStringLiteral("main") });
    }

    void movesAndRebasesBranchesThatAreNotCheckedOut()
    {
        RepositoryController controller;
        controller.openRepositoryPath(m_root);
        settle(controller);
        const auto head = controller.headOid();
        QVERIFY(controller.history()->reachableFromHead(git({ QStringLiteral("rev-parse"), QStringLiteral("HEAD~2") })));
        QVERIFY(!controller.history()->reachableFromHead(git({ QStringLiteral("rev-parse"), QStringLiteral("topic") })));

        controller.history()->rebaseBranch(QStringLiteral("topic"), QStringLiteral("main"));
        settle(controller);
        QCOMPARE(git({ QStringLiteral("rev-parse"), QStringLiteral("topic~1") }), head);

        controller.history()->mergeBranchInto(QStringLiteral("main"), QStringLiteral("topic"));
        settle(controller);
        QCOMPARE(controller.currentBranch(), QStringLiteral("topic"));

        controller.history()->moveBranch(QStringLiteral("main"), git({ QStringLiteral("rev-parse"), QStringLiteral("HEAD") }));
        settle(controller);
        QCOMPARE(git({ QStringLiteral("rev-parse"), QStringLiteral("main") }), git({ QStringLiteral("rev-parse"), QStringLiteral("topic") }));
        QVERIFY(controller.lastOperationOk());
    }

    void plansAndRunsAnInteractiveRebase()
    {
        RepositoryController controller;
        controller.openRepositoryPath(m_root);
        settle(controller);
        QSignalSpy plans(controller.history(), &HistoryEditor::rebasePlanReady);
        controller.history()->requestRebasePlan(git({ QStringLiteral("rev-parse"), QStringLiteral("HEAD~1") }));
        QTRY_COMPARE_WITH_TIMEOUT(plans.size(), 1, 5000);
        auto plan = plans.first().first().toMap();
        QVERIFY(plan.value(QStringLiteral("ok")).toBool());
        auto steps = plan.value(QStringLiteral("steps")).toList();
        QCOMPARE(steps.size(), 2);
        auto first = steps.at(0).toMap();
        first.insert(QStringLiteral("action"), QStringLiteral("reword"));
        first.insert(QStringLiteral("message"), QStringLiteral("renamed"));
        auto second = steps.at(1).toMap();
        second.insert(QStringLiteral("action"), QStringLiteral("squash"));
        controller.history()->startInteractiveRebase(plan.value(QStringLiteral("base")).toString(), { first, second }, false);
        settle(controller);
        QVERIFY2(controller.lastOperationOk(), qPrintable(controller.operationMessage()));
        QCOMPARE(git({ QStringLiteral("log"), QStringLiteral("-1"), QStringLiteral("--format=%B") }),
                 QStringLiteral("renamed\n\nmain-two"));
    }

    void hiddenRefsLoseTheirChipsUntilShownAgain()
    {
        RepositoryController controller;
        controller.openRepositoryPath(m_root);
        settle(controller);
        runGit(m_root, { QStringLiteral("tag"), QStringLiteral("v1"), QStringLiteral("HEAD~1") });
        controller.refresh();
        settle(controller);
        QVERIFY(wearsChip(controller, QStringLiteral("v1")));
        controller.setRefHidden(QStringLiteral("refs/tags/v1"), true);
        controller.setRefHidden(QStringLiteral("refs/heads/topic"), true);
        settle(controller);
        QVERIFY(!wearsChip(controller, QStringLiteral("v1")));
        QVERIFY(!wearsChip(controller, QStringLiteral("topic")));
        QCOMPARE(controller.hiddenRefs().size(), 2);
        controller.setRefHidden(QStringLiteral("refs/heads/main"), true);
        settle(controller);
        QVERIFY(wearsChip(controller, QStringLiteral("main")));
        controller.showAllRefs();
        settle(controller);
        QVERIFY(controller.hiddenRefs().isEmpty());
        QVERIFY(wearsChip(controller, QStringLiteral("topic")));
    }
};

QTEST_GUILESS_MAIN(BranchOperationsTest)
#include "branch_operations_test.moc"
