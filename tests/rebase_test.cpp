#include "git_rebase.hpp"
#include "git_test_helpers.hpp"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

using namespace GitNaga;
using rebase::Action;

// Runs every rebase arrangement against a real repository, the way the
// editor and the quick actions do, and reads the rewritten history back.
class RebaseTest final : public QObject
{
    Q_OBJECT

    QTemporaryDir m_directory;
    QString m_root;

    static QString git(const QString &root, const QStringList &arguments)
    {
        auto output = GitClient::mutate(root, arguments, QStringLiteral("test"));
        return output ? *output : QStringLiteral("<failed: %1>").arg(output.error().message);
    }

    QString subjects() const
    {
        return git(m_root, { QStringLiteral("log"), QStringLiteral("--reverse"), QStringLiteral("--format=%s") })
            .replace(QLatin1Char('\n'), QLatin1Char(','));
    }

    QString message(const QString &revision) const
    {
        return git(m_root, { QStringLiteral("log"), QStringLiteral("-1"), QStringLiteral("--format=%B"), revision });
    }

    QString files() const
    {
        return git(m_root, { QStringLiteral("ls-tree"), QStringLiteral("--name-only"), QStringLiteral("HEAD") })
            .replace(QLatin1Char('\n'), QLatin1Char(','));
    }

    // Every step of the range above the root, as the editor would load it.
    gitrebase::Range range(int fromCommit = 2) const
    {
        const auto oid = git(m_root, { QStringLiteral("rev-parse"), QStringLiteral("HEAD~%1").arg(5 - fromCommit) });
        auto loaded = gitrebase::loadRange(m_root, oid);
        return loaded ? *loaded : gitrebase::Range{};
    }

private slots:
    void init()
    {
        QVERIFY(m_directory.isValid());
        m_root = m_directory.path() + QStringLiteral("/repo");
        QDir().mkpath(m_root);
        QDir(m_root).removeRecursively();
        QDir().mkpath(m_root);
        runGit(m_root, { QStringLiteral("init"), QStringLiteral("-q"), QStringLiteral("-b"), QStringLiteral("main") });
        runGit(m_root, { QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("GitNaga Test") });
        runGit(m_root, { QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@gitnaga.invalid") });
        runGit(m_root, { QStringLiteral("config"), QStringLiteral("commit.gpgsign"), QStringLiteral("false") });
        const QStringList names{ QStringLiteral("one"), QStringLiteral("two"), QStringLiteral("three"),
                                 QStringLiteral("four"), QStringLiteral("five") };
        for (const auto &name : names) {
            writeFile(m_root + QLatin1Char('/') + name + QStringLiteral(".txt"), name.toUtf8() + '\n');
            runGit(m_root, { QStringLiteral("add"), QStringLiteral(".") });
            runGit(m_root, { QStringLiteral("commit"), QStringLiteral("-qm"), name, QStringLiteral("-m"),
                             QStringLiteral("body of ") + name });
        }
    }

    void loadsTheRangeOldestFirst()
    {
        const auto loaded = range();
        QCOMPARE(loaded.steps.size(), 4);
        QCOMPARE(loaded.steps.first().subject, QStringLiteral("two"));
        QCOMPARE(loaded.steps.first().original, QStringLiteral("two\n\nbody of two\n"));
        QCOMPARE(loaded.base, git(m_root, { QStringLiteral("rev-parse"), QStringLiteral("HEAD~4") }));
    }

    void reorders()
    {
        auto loaded = range();
        loaded.steps.swapItemsAt(0, 2);
        QVERIFY(gitrebase::run(m_root, loaded.base, loaded.steps, false).has_value());
        QCOMPARE(subjects(), QStringLiteral("one,four,three,two,five"));
        QCOMPARE(files(), QStringLiteral("five.txt,four.txt,one.txt,three.txt,two.txt"));
    }

    void rewordsAndSquashesInOneRun()
    {
        auto loaded = range();
        loaded.steps[0].action = Action::Reword;
        loaded.steps[0].message = QStringLiteral("second, renamed\n\nnew body");
        loaded.steps[2].action = Action::Squash;
        loaded.steps[3].action = Action::Fixup;
        QVERIFY(gitrebase::run(m_root, loaded.base, loaded.steps, false).has_value());
        QCOMPARE(subjects(), QStringLiteral("one,second, renamed,three"));
        QCOMPARE(message(QStringLiteral("HEAD")), QStringLiteral("three\n\nbody of three\n\nfour\n\nbody of four"));
        QCOMPARE(files(), QStringLiteral("five.txt,four.txt,one.txt,three.txt,two.txt"));
    }

    void dropsAndFixesUp()
    {
        auto loaded = range();
        loaded.steps[1].action = Action::Drop;
        loaded.steps[2].action = Action::Fixup;
        QVERIFY(gitrebase::run(m_root, loaded.base, loaded.steps, false).has_value());
        QCOMPARE(subjects(), QStringLiteral("one,two,five"));
        QCOMPARE(message(QStringLiteral("HEAD~1")), QStringLiteral("two\n\nbody of two"));
        QCOMPARE(files(), QStringLiteral("five.txt,four.txt,one.txt,two.txt"));
    }

    void messagesSurviveAStop()
    {
        auto loaded = range();
        loaded.steps[0].action = Action::Edit;
        loaded.steps[2].action = Action::Reword;
        loaded.steps[2].message = QStringLiteral("four, after the stop");
        QVERIFY(gitrebase::run(m_root, loaded.base, loaded.steps, false).has_value());
        QVERIFY(QDir(m_root + QStringLiteral("/.git/rebase-merge")).exists());
        runGit(m_root, { QStringLiteral("rebase"), QStringLiteral("--continue") });
        QCOMPARE(subjects(), QStringLiteral("one,two,three,four, after the stop,five"));
        QVERIFY(!QDir(m_root + QStringLiteral("/.git/rebase-merge")).exists());
    }

    void refusesADirtyWorktreeUnlessAutostashing()
    {
        writeFile(m_root + QStringLiteral("/one.txt"), "changed\n");
        auto loaded = range();
        loaded.steps[3].action = Action::Drop;
        const auto refused = gitrebase::run(m_root, loaded.base, loaded.steps, false);
        QVERIFY(!refused.has_value());
        QVERIFY(refused.error().message.contains(QStringLiteral("autostash")));
        QCOMPARE(subjects(), QStringLiteral("one,two,three,four,five"));
        QVERIFY(gitrebase::run(m_root, loaded.base, loaded.steps, true).has_value());
        QCOMPARE(subjects(), QStringLiteral("one,two,three,four"));
        QCOMPARE(git(m_root, { QStringLiteral("status"), QStringLiteral("--porcelain") }), QStringLiteral("M one.txt"));
    }

    void refusesMerges()
    {
        runGit(m_root, { QStringLiteral("switch"), QStringLiteral("-qc"), QStringLiteral("side"), QStringLiteral("HEAD~1") });
        writeFile(m_root + QStringLiteral("/side.txt"), "side\n");
        runGit(m_root, { QStringLiteral("add"), QStringLiteral(".") });
        runGit(m_root, { QStringLiteral("commit"), QStringLiteral("-qm"), QStringLiteral("side") });
        runGit(m_root, { QStringLiteral("switch"), QStringLiteral("-q"), QStringLiteral("main") });
        runGit(m_root, { QStringLiteral("merge"), QStringLiteral("-q"), QStringLiteral("--no-ff"), QStringLiteral("side"),
                         QStringLiteral("-m"), QStringLiteral("merge side") });
        const auto loaded = gitrebase::loadRange(m_root, git(m_root, { QStringLiteral("rev-parse"), QStringLiteral("HEAD~3") }));
        QVERIFY(!loaded.has_value());
        QVERIFY(loaded.error().message.contains(QStringLiteral("merge")));
    }

    void quickActionsReachTheRoot()
    {
        const auto rootOid = git(m_root, { QStringLiteral("rev-list"), QStringLiteral("--max-parents=0"), QStringLiteral("HEAD") });
        QVERIFY(gitrebase::reword(m_root, rootOid, QStringLiteral("first")).has_value());
        QCOMPARE(subjects(), QStringLiteral("first,two,three,four,five"));
        const auto two = git(m_root, { QStringLiteral("rev-parse"), QStringLiteral("HEAD~3") });
        QVERIFY(gitrebase::squashIntoParent(m_root, two).has_value());
        QCOMPARE(subjects(), QStringLiteral("first,three,four,five"));
        QCOMPARE(message(QStringLiteral("HEAD~3")), QStringLiteral("first\n\ntwo\n\nbody of two"));
        QVERIFY(gitrebase::drop(m_root, git(m_root, { QStringLiteral("rev-parse"), QStringLiteral("HEAD~1") })).has_value());
        QCOMPARE(subjects(), QStringLiteral("first,three,five"));
        QVERIFY(!gitrebase::squashIntoParent(m_root, git(m_root, { QStringLiteral("rev-parse"), QStringLiteral("HEAD~2") }))
                     .has_value());
    }
};

QTEST_GUILESS_MAIN(RebaseTest)
#include "rebase_test.moc"
