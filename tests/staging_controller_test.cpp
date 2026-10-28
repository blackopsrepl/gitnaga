#include "git_client.hpp"
#include "git_test_helpers.hpp"
#include "repository_controller.hpp"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace GitNaga;

// Staging driven the way the review pane drives it: rows of the two lists,
// hunk headers and line rows of the loaded diff.
class StagingControllerTest final : public QObject
{
    Q_OBJECT

    QTemporaryDir m_directory;
    QString m_root;

    QString git(const QStringList &arguments) const
    {
        return GitClient::mutate(m_root, arguments, QStringLiteral("test")).value_or(QString());
    }

    QString indexed(const QString &path) const { return git({ QStringLiteral("show"), QLatin1Char(':') + path }); }

    static QString numbered(int changeAt = -1, int changeTwo = -1)
    {
        QString text;
        for (int line = 1; line <= 20; ++line)
            text += (line == changeAt || line == changeTwo ? QStringLiteral("changed ") : QString())
                    + QString::number(line) + QLatin1Char('\n');
        return text;
    }

    // Opens the repository with the work-in-progress row selected.
    void open(RepositoryController &controller)
    {
        controller.openRepositoryPath(m_root);
        QTRY_COMPARE_WITH_TIMEOUT(controller.workInProgressOid(), worktreeOid(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.historyLoading(), 5000);
        controller.selectOid(worktreeOid());
    }

    static QVector<int> rowsOfKind(RepositoryController &controller, DiffLine::Kind kind, const QString &text = {})
    {
        QVector<int> rows;
        const auto &shown = controller.diffLines()->document().rows;
        for (int row = 0; row < shown.size(); ++row)
            if (shown.at(row).kind == kind && (text.isEmpty() || shown.at(row).text == text))
                rows.append(kind == DiffLine::Kind::Hunk ? shown.at(row).hunk : row);
        return rows;
    }

    static bool waitFor(QSignalSpy &finished, int count)
    {
        if (!QTest::qWaitFor([&] { return finished.count() >= count; }, 10000))
            return false;
        return finished.last().at(0).toBool();
    }

private slots:
    void init()
    {
        m_root = m_directory.path() + QLatin1Char('/') + QString::fromUtf8(QTest::currentTestFunction());
        QVERIFY(QDir().mkpath(m_root));
        runGit(m_root, { QStringLiteral("init"), QStringLiteral("-q"), QStringLiteral("-b"), QStringLiteral("main") });
        runGit(m_root, { QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("GitNaga Test") });
        runGit(m_root, { QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@gitnaga.invalid") });
        writeFile(m_root + QStringLiteral("/file.txt"), numbered().toUtf8());
        runGit(m_root, { QStringLiteral("add"), QStringLiteral("file.txt") });
        runGit(m_root, { QStringLiteral("commit"), QStringLiteral("-qm"), QStringLiteral("base") });
    }

    void movesWholeFiles()
    {
        writeFile(m_root + QStringLiteral("/file.txt"), numbered(2).toUtf8());
        writeFile(m_root + QStringLiteral("/new.txt"), QByteArrayLiteral("new\n"));
        RepositoryController controller;
        QSignalSpy finished(&controller, &RepositoryController::operationFinished);
        open(controller);
        QCOMPARE(controller.unstagedFiles()->count(), 2);
        QCOMPARE(controller.stagedFiles()->count(), 0);
        QCOMPARE(controller.wipSide(), QStringLiteral("unstaged"));

        // The file being read follows itself to the staged side.
        controller.selectWorktreeFile(QStringLiteral("unstaged"), 0);
        controller.stageFile(0);
        QVERIFY(waitFor(finished, 1));
        QTRY_COMPARE_WITH_TIMEOUT(controller.stagedFiles()->count(), 1, 5000);
        QCOMPARE(controller.stagedCount(), 1);
        QCOMPARE(controller.wipSide(), QStringLiteral("staged"));
        QTRY_VERIFY_WITH_TIMEOUT(!rowsOfKind(controller, DiffLine::Kind::Addition, QStringLiteral("+changed 2")).isEmpty(), 5000);

        controller.unstageFile(0);
        QVERIFY(waitFor(finished, 2));
        QTRY_COMPARE_WITH_TIMEOUT(controller.stagedFiles()->count(), 0, 5000);
        QCOMPARE(controller.wipSide(), QStringLiteral("unstaged"));

        // Discarding an untracked file deletes it.
        controller.discardFile(1);
        QVERIFY(waitFor(finished, 3));
        QVERIFY(!QFile::exists(m_root + QStringLiteral("/new.txt")));
        controller.discardAll();
        QVERIFY(waitFor(finished, 4));
        QTRY_COMPARE_WITH_TIMEOUT(controller.workInProgressOid(), QString(), 5000);
    }

    void movesHunksAndLines()
    {
        writeFile(m_root + QStringLiteral("/file.txt"), numbered(2, 19).toUtf8());
        RepositoryController controller;
        QSignalSpy finished(&controller, &RepositoryController::operationFinished);
        open(controller);
        QTRY_COMPARE_WITH_TIMEOUT(rowsOfKind(controller, DiffLine::Kind::Hunk).size(), 2, 5000);

        controller.applyHunk(rowsOfKind(controller, DiffLine::Kind::Hunk).front(), QStringLiteral("stage"));
        QVERIFY(waitFor(finished, 1));
        QCOMPARE(indexed(QStringLiteral("file.txt")) + QLatin1Char('\n'), numbered(2));
        // Half the file is still unstaged, so the view stays there with one hunk.
        QCOMPARE(controller.wipSide(), QStringLiteral("unstaged"));
        QTRY_COMPARE_WITH_TIMEOUT(rowsOfKind(controller, DiffLine::Kind::Hunk).size(), 1, 5000);

        // Discard the remaining hunk's removed line only: line 19 comes back
        // next to its changed copy.
        controller.applyLines({ rowsOfKind(controller, DiffLine::Kind::Deletion).front() }, QStringLiteral("discard"));
        QVERIFY(waitFor(finished, 2));
        QFile file(m_root + QStringLiteral("/file.txt"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.readAll().contains("19\nchanged 19\n"));

        // Unstage the added line alone: the staged removal of "2" stays.
        QTRY_COMPARE_WITH_TIMEOUT(controller.stagedFiles()->count(), 1, 5000);
        controller.selectWorktreeFile(QStringLiteral("staged"), 0);
        QTRY_VERIFY_WITH_TIMEOUT(!rowsOfKind(controller, DiffLine::Kind::Addition, QStringLiteral("+changed 2")).isEmpty(), 5000);
        controller.applyLines({ rowsOfKind(controller, DiffLine::Kind::Addition).front() }, QStringLiteral("unstage"));
        QVERIFY(waitFor(finished, 3));
        QCOMPARE(indexed(QStringLiteral("file.txt")), QStringLiteral("1\n") + numbered().mid(4).trimmed());
    }

    void stagesLinesOfAnUntrackedFile()
    {
        writeFile(m_root + QStringLiteral("/new.txt"), QByteArrayLiteral("a\nb\n"));
        RepositoryController controller;
        QSignalSpy finished(&controller, &RepositoryController::operationFinished);
        open(controller);
        QTRY_VERIFY_WITH_TIMEOUT(!rowsOfKind(controller, DiffLine::Kind::Addition, QStringLiteral("+a")).isEmpty(), 5000);
        controller.applyLines({ rowsOfKind(controller, DiffLine::Kind::Addition, QStringLiteral("+a")).front() },
                              QStringLiteral("stage"));
        QVERIFY(waitFor(finished, 1));
        QCOMPARE(indexed(QStringLiteral("new.txt")), QStringLiteral("a"));
        // The rest of the file is now an ordinary unstaged change.
        QTRY_COMPARE_WITH_TIMEOUT(controller.stagedFiles()->count(), 1, 5000);
        QCOMPARE(controller.unstagedFiles()->count(), 1);
    }

    void commitsAndAmends()
    {
        writeFile(m_root + QStringLiteral("/file.txt"), numbered(5).toUtf8());
        RepositoryController controller;
        QSignalSpy finished(&controller, &RepositoryController::operationFinished);
        QSignalSpy loaded(&controller, &RepositoryController::amendMessageLoaded);
        open(controller);
        // Nothing staged: a plain commit is refused before git is asked.
        controller.commitWorktree(QStringLiteral("nothing"), QString());
        QCOMPARE(finished.count(), 1);
        QVERIFY(!finished.last().at(0).toBool());

        controller.stageAll();
        QVERIFY(waitFor(finished, 2));
        QTRY_COMPARE_WITH_TIMEOUT(controller.stagedCount(), 1, 5000);
        controller.commitWorktree(QStringLiteral("five"), QStringLiteral("why"));
        QVERIFY(waitFor(finished, 3));
        controller.loadAmendMessage();
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 5000);
        QCOMPARE(loaded.last().at(0).toString(), QStringLiteral("five"));
        QCOMPARE(loaded.last().at(1).toString(), QStringLiteral("why"));
        QVERIFY(!loaded.last().at(2).toBool());
        QTRY_COMPARE_WITH_TIMEOUT(controller.stagedCount(), 0, 5000);
        controller.commitWorktree(QStringLiteral("five, reworded"), QString(), true);
        QVERIFY(waitFor(finished, 4));
        QCOMPARE(git({ QStringLiteral("log"), QStringLiteral("--format=%s") }), QStringLiteral("five, reworded\nbase"));
    }

    void reviewsAndPopsStashes()
    {
        writeFile(m_root + QStringLiteral("/file.txt"), numbered(7).toUtf8());
        writeFile(m_root + QStringLiteral("/loose.txt"), QByteArrayLiteral("loose\n"));
        RepositoryController controller;
        QSignalSpy finished(&controller, &RepositoryController::operationFinished);
        open(controller);
        controller.stashWorktree(QStringLiteral("parked"), true);
        QVERIFY(waitFor(finished, 1));
        QTRY_COMPARE_WITH_TIMEOUT(controller.stashes().size(), 1, 5000);
        const auto stash = controller.stashes().front().toMap();
        QVERIFY(stash.value(QStringLiteral("message")).toString().contains(QStringLiteral("parked")));

        // The stash shows its change against its base and the untracked file.
        controller.selectOid(stash.value(QStringLiteral("oid")).toString());
        QTRY_COMPARE_WITH_TIMEOUT(controller.changedFiles()->count(), 2, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!rowsOfKind(controller, DiffLine::Kind::Addition, QStringLiteral("+changed 7")).isEmpty(), 5000);
        controller.selectFile(1);
        QTRY_VERIFY_WITH_TIMEOUT(!rowsOfKind(controller, DiffLine::Kind::Addition, QStringLiteral("+loose")).isEmpty(), 5000);

        // A stale selector is refused; the right one pops.
        controller.stashAction(QStringLiteral("drop"), QStringLiteral("stash@{0}"), QStringLiteral("0000"));
        QVERIFY(!waitFor(finished, 2));
        controller.stashAction(QStringLiteral("pop"), QStringLiteral("stash@{0}"), stash.value(QStringLiteral("oid")).toString());
        QVERIFY(waitFor(finished, 3));
        QVERIFY(QFile::exists(m_root + QStringLiteral("/loose.txt")));
        QTRY_COMPARE_WITH_TIMEOUT(controller.stashes().size(), 0, 5000);
    }
};

QTEST_GUILESS_MAIN(StagingControllerTest)

#include "staging_controller_test.moc"
