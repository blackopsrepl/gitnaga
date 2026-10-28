#include "git_client.hpp"
#include "git_test_helpers.hpp"
#include "repository_controller.hpp"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

using namespace GitNaga;

// The controller is where the application's state lives, and where the bugs a
// person sees come from: a selection that vanished, a stale worktree row, a
// click that was dropped. These drive it the way the window does.
class ControllerTest final : public QObject
{
    Q_OBJECT

    static QString makeRepository(const QString &root)
    {
        runGit(root, { QStringLiteral("init"), QStringLiteral("-b"), QStringLiteral("main") });
        runGit(root, { QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("GitNaga Test") });
        runGit(root, { QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@gitnaga.invalid") });
        for (const auto &line : { QByteArrayLiteral("one\n"), QByteArrayLiteral("two\n") }) {
            writeFile(root + QStringLiteral("/file.txt"), line);
            runGit(root, { QStringLiteral("add"), QStringLiteral("file.txt") });
            runGit(root, { QStringLiteral("commit"), QStringLiteral("-m"), QString::fromUtf8(line).trimmed() });
        }
        return root;
    }

    static QString objectCount(const QString &root)
    {
        return *GitClient::mutate(root, { QStringLiteral("count-objects") }, QStringLiteral("count"));
    }

    static bool diffContains(RepositoryController &controller, const QString &text)
    {
        auto *model = controller.diffLines();
        for (int row = 0; row < model->rowCount(); ++row) {
            if (model->data(model->index(row), DiffLineModel::TextRole).toString() == text)
                return true;
        }
        return false;
    }

private slots:
    void worktreeSelectionHoldsAndWritesNothing()
    {
        QTemporaryDir directory;
        const auto root = makeRepository(directory.path());
        writeFile(root + QStringLiteral("/file.txt"), QByteArrayLiteral("three\n"));
        const auto objects = objectCount(root);

        RepositoryController controller;
        controller.openRepositoryPath(root);
        QTRY_COMPARE_WITH_TIMEOUT(controller.workInProgressOid(), worktreeOid(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.historyLoading(), 5000);
        controller.selectOid(worktreeOid());
        QCOMPARE(controller.selectedOid(), worktreeOid());
        QCOMPARE(controller.selectedRow(), 0);
        // The diff is the working change, not HEAD's own commit.
        QTRY_VERIFY_WITH_TIMEOUT(diffContains(controller, QStringLiteral("+three")), 5000);
        QVERIFY(diffContains(controller, QStringLiteral("-two")));

        // Polls and watcher events come and go; the selection stays put and
        // the repository gains no objects from being looked at.
        QTest::qWait(3000);
        QCOMPARE(controller.selectedOid(), worktreeOid());
        QCOMPARE(controller.selectedRow(), 0);
        QCOMPARE(objectCount(root), objects);
    }

    void editsAppearWithoutARefresh()
    {
        QTemporaryDir directory;
        const auto root = makeRepository(directory.path());
        RepositoryController controller;
        controller.openRepositoryPath(root);
        QTRY_COMPARE_WITH_TIMEOUT(controller.commitCount(), 2, 5000);
        QVERIFY(controller.workInProgressOid().isEmpty());
        writeFile(root + QStringLiteral("/new.txt"), QByteArrayLiteral("new\n"));
        QTRY_COMPARE_WITH_TIMEOUT(controller.workInProgressOid(), worktreeOid(), 6000);
        QCOMPARE(controller.uncommittedCount(), 1);
    }

    void selectionSurvivesAnExternalCommit()
    {
        QTemporaryDir directory;
        const auto root = makeRepository(directory.path());
        RepositoryController controller;
        controller.openRepositoryPath(root);
        QTRY_COMPARE_WITH_TIMEOUT(controller.commitCount(), 2, 5000);
        controller.selectCommit(1);
        const auto selected = controller.selectedOid();
        QVERIFY(!selected.isEmpty());

        writeFile(root + QStringLiteral("/file.txt"), QByteArrayLiteral("four\n"));
        runGit(root, { QStringLiteral("commit"), QStringLiteral("-am"), QStringLiteral("four") });
        QTRY_COMPARE_WITH_TIMEOUT(controller.commitCount(), 3, 6000);
        QCOMPARE(controller.selectedOid(), selected);
        QCOMPARE(controller.selectedRow(), 2);
    }

    void queuedOperationsAllRun()
    {
        QTemporaryDir directory;
        const auto root = makeRepository(directory.path());
        RepositoryController controller;
        QSignalSpy finished(&controller, &RepositoryController::operationFinished);
        controller.openRepositoryPath(root);
        QTRY_COMPARE_WITH_TIMEOUT(controller.commitCount(), 2, 5000);
        const auto head = controller.headOid();
        // Three clicks in a row: none of them may be dropped.
        controller.createTag(QStringLiteral("a"), head);
        controller.createTag(QStringLiteral("b"), head);
        controller.createBranch(QStringLiteral("c"), head);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 3, 10000);
        for (const auto &args : finished)
            QVERIFY(args.at(0).toBool());
        const auto refs = *GitClient::mutate(root, { QStringLiteral("for-each-ref"), QStringLiteral("--format=%(refname)") },
                                             QStringLiteral("refs"));
        QVERIFY(refs.contains(QStringLiteral("refs/tags/a")));
        QVERIFY(refs.contains(QStringLiteral("refs/tags/b")));
        QVERIFY(refs.contains(QStringLiteral("refs/heads/c")));
    }

    void annotatedTagsAreReferences()
    {
        QTemporaryDir directory;
        const auto root = makeRepository(directory.path());
        runGit(root, { QStringLiteral("tag"), QStringLiteral("-a"), QStringLiteral("v1"), QStringLiteral("-m"), QStringLiteral("one") });
        RepositoryController controller;
        controller.openRepositoryPath(root);
        QTRY_COMPARE_WITH_TIMEOUT(controller.commitCount(), 2, 5000);
        bool found = false;
        for (const auto &entry : controller.references()) {
            const auto map = entry.toMap();
            if (map.value(QStringLiteral("kind")) == QStringLiteral("tag") && map.value(QStringLiteral("name")) == QStringLiteral("v1"))
                found = map.value(QStringLiteral("oid")).toString() == controller.headOid();
        }
        QVERIFY(found);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.historyLoading(), 5000);
        const auto *top = controller.commits()->commitAt(0);
        QVERIFY(top);
        QVERIFY(std::ranges::any_of(top->refs, [](const Ref &ref) { return ref.kind == RefKind::Tag; }));
    }
};

QTEST_GUILESS_MAIN(ControllerTest)

#include "controller_test.moc"
