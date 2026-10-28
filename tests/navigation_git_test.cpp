#include "git_navigation.hpp"
#include "commit_search.hpp"
#include "git_test_helpers.hpp"
#include "navigation_controller.hpp"
#include "repository_controller.hpp"

#include <QTemporaryDir>
#include <QTest>

using namespace GitNaga;

// Search, file history and blame against a real repository: git decides
// what path and content questions mean, so only git can confirm the answers.
class NavigationGitTest final : public QObject
{
    Q_OBJECT

    QTemporaryDir m_directory;
    QString m_root;
    QHash<QString, QString> m_oids;

    void commit(const QString &subject)
    {
        runGit(m_root, { QStringLiteral("add"), QStringLiteral("-A") });
        runGit(m_root, { QStringLiteral("commit"), QStringLiteral("-qm"), subject });
        m_oids.insert(subject, *GitClient::mutate(m_root, { QStringLiteral("rev-parse"), QStringLiteral("HEAD") },
                                                  QStringLiteral("rev-parse")));
    }

    QStringList subjectsOf(const QStringList &oids) const
    {
        QStringList subjects;
        for (const auto &oid : oids)
            subjects.append(m_oids.key(oid));
        return subjects;
    }

    QStringList revisions() const
    {
        return GitClient::historyRevisions(GitClient::loadRefs(*GitClient::locate(m_root)).value(), {});
    }

private slots:
    void initTestCase()
    {
        m_root = m_directory.path();
        runGit(m_root, { QStringLiteral("init"), QStringLiteral("-qb"), QStringLiteral("main") });
        runGit(m_root, { QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("Ada Lovelace") });
        runGit(m_root, { QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("ada@zoo.invalid") });
        runGit(m_root, { QStringLiteral("config"), QStringLiteral("commit.gpgsign"), QStringLiteral("false") });
        writeFile(m_root + QStringLiteral("/a.txt"), "alpha\nbeta\ngamma\ndelta\n");
        commit(QStringLiteral("add a"));
        writeFile(m_root + QStringLiteral("/other.txt"), "nothing\n");
        commit(QStringLiteral("add other"));
        QDir(m_root).mkdir(QStringLiteral("dir"));
        runGit(m_root, { QStringLiteral("mv"), QStringLiteral("a.txt"), QStringLiteral("dir/b.txt") });
        commit(QStringLiteral("rename a"));
        writeFile(m_root + QStringLiteral("/dir/b.txt"), "alpha\nbeta\ngamma\ndelta\nneedle line\n");
        commit(QStringLiteral("edit b"));
        writeFile(m_root + QStringLiteral("/other.txt"), "nothing\nunrelated\n");
        commit(QStringLiteral("touch other"));
        writeFile(m_root + QStringLiteral("/dir/b.txt"), "alpha\nbeta\ngamma\ndelta\nneedle line\ndraft\n");
    }

    void pathQueriesFollowPathspecs()
    {
        const auto plain = searchHistory(m_root, revisions(), parseSearchQuery(QStringLiteral("path:other.txt")));
        QVERIFY(plain);
        QCOMPARE(subjectsOf(*plain), (QStringList{ QStringLiteral("touch other"), QStringLiteral("add other") }));
        const auto glob = searchHistory(m_root, revisions(), parseSearchQuery(QStringLiteral("path:dir/*")));
        QVERIFY(glob);
        QCOMPARE(subjectsOf(*glob), (QStringList{ QStringLiteral("edit b"), QStringLiteral("rename a") }));
        const auto directory = searchHistory(m_root, revisions(), parseSearchQuery(QStringLiteral("path:dir")));
        QCOMPARE(directory->size(), 2);
    }

    void contentQueriesUsePickaxe()
    {
        const auto hits = searchHistory(m_root, revisions(), parseSearchQuery(QStringLiteral("content:needle")));
        QVERIFY(hits);
        QCOMPARE(subjectsOf(*hits), QStringList{ QStringLiteral("edit b") });
        const auto none = searchHistory(m_root, revisions(), parseSearchQuery(QStringLiteral("content:absent")));
        QVERIFY(none && none->isEmpty());
    }

    void fileHistoryFollowsARename()
    {
        const auto history = fileHistory(m_root, {}, QStringLiteral("dir/b.txt"));
        QVERIFY(history);
        QCOMPARE(history->size(), 3);
        QCOMPARE(history->at(0).subject, QStringLiteral("edit b"));
        QCOMPARE(history->at(1).status, QStringLiteral("R"));
        QCOMPARE(history->at(1).oldPath, QStringLiteral("a.txt"));
        QCOMPARE(history->at(1).path, QStringLiteral("dir/b.txt"));
        QCOMPARE(history->at(2).path, QStringLiteral("a.txt"));
        QCOMPARE(history->at(2).oid, m_oids.value(QStringLiteral("add a")));
    }

    void blameAttributesWorktreeLines()
    {
        const auto lines = blameFile(m_root, {}, QStringLiteral("dir/b.txt"));
        QVERIFY(lines);
        QCOMPARE(lines->size(), 6);
        QCOMPARE(lines->at(0).oid, m_oids.value(QStringLiteral("add a")));
        QCOMPARE(lines->at(4).oid, m_oids.value(QStringLiteral("edit b")));
        QCOMPARE(lines->at(4).summary, QStringLiteral("edit b"));
        QVERIFY(lines->at(5).uncommitted());
        const auto atCommit = blameFile(m_root, m_oids.value(QStringLiteral("rename a")), QStringLiteral("dir/b.txt"));
        QVERIFY(atCommit);
        QCOMPARE(atCommit->size(), 4);
        QVERIFY(!blameFile(m_root, {}, QStringLiteral("missing.txt")));
    }

    void controllerWalksSearchHits()
    {
        RepositoryController repository;
        NavigationController navigation(&repository);
        repository.openRepositoryPath(m_root);
        QTRY_VERIFY_WITH_TIMEOUT(repository.commitCount() >= 5 && !repository.historyLoading(), 5000);
        auto *search = navigation.search();
        search->setQuery(QStringLiteral("path:other.txt"));
        search->runNow();
        QTRY_COMPARE_WITH_TIMEOUT(search->count(), 2, 5000);
        // A new question lands on its first answer.
        QTRY_COMPARE_WITH_TIMEOUT(repository.selectedOid(), m_oids.value(QStringLiteral("touch other")), 2000);
        QCOMPARE(search->position(), 1);
        search->next();
        QCOMPARE(repository.selectedOid(), m_oids.value(QStringLiteral("add other")));
        QCOMPARE(search->position(), 2);
        search->next();
        QCOMPARE(search->position(), 1);
        search->previous();
        QCOMPARE(search->position(), 2);
        QVERIFY(search->isMatch(repository.selectedRow()));
        QVERIFY(!search->isMatch(repository.commits()->rowOf(m_oids.value(QStringLiteral("edit b")))));

        search->setQuery(QStringLiteral("content:needle"));
        search->runNow();
        QTRY_VERIFY_WITH_TIMEOUT(!search->running() && search->count() == 1, 5000);
        search->setQuery(QStringLiteral("author:ada"));
        search->runNow();
        QTRY_VERIFY_WITH_TIMEOUT(!search->running() && search->count() == 5, 5000);
        search->clear();
        QVERIFY(!search->active());
        QCOMPARE(search->count(), 0);

        const int rename = repository.commits()->rowOf(m_oids.value(QStringLiteral("rename a")));
        const int other = repository.commits()->rowOf(m_oids.value(QStringLiteral("add other")));
        QCOMPARE(navigation.parentRow(rename), other);
        QCOMPARE(navigation.childRow(other), rename);
        QCOMPARE(navigation.resolveCommit(m_oids.value(QStringLiteral("edit b")).left(10)),
                 m_oids.value(QStringLiteral("edit b")));
        QCOMPARE(navigation.resolveCommit(QStringLiteral("zz")), QString());
    }

    void fileHistoryShowsTheRenameDiff()
    {
        RepositoryController repository;
        NavigationController navigation(&repository);
        repository.openRepositoryPath(m_root);
        QTRY_VERIFY_WITH_TIMEOUT(!repository.historyLoading() && repository.commitCount() >= 5, 5000);
        navigation.showFileHistory(QStringLiteral("dir/b.txt"), worktreeOid());
        auto *history = navigation.fileHistory();
        QTRY_COMPARE_WITH_TIMEOUT(history->count(), 3, 5000);
        history->select(1);
        auto *diff = history->diffLines();
        QTRY_VERIFY_WITH_TIMEOUT(diff->rowCount() > 0, 5000);
        bool renamed = false;
        for (int row = 0; row < diff->rowCount(); ++row)
            renamed |= diff->data(diff->index(row), DiffLineModel::TextRole).toString() == QStringLiteral("rename from a.txt");
        QVERIFY(renamed);
    }
};

QTEST_GUILESS_MAIN(NavigationGitTest)

#include "navigation_git_test.moc"
