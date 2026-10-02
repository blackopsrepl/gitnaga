#include "git_refs.hpp"

#include <QTest>

// Naming contracts for remote-tracking refs, kept out of the client test so
// each file owns one concern and stays well inside the source-size cap.

using namespace GitNaga;

class GitRefsTest final : public QObject
{
    Q_OBJECT

private slots:
    void splitsTheRemoteFromTheBranch()
    {
        QString remote;
        QString branch;

        // The remote is the segment before the first slash; the branch is the
        // rest of the name, slashes and all.
        QVERIFY(gitrefs::splitRemoteRef(QStringLiteral("github/main"), &remote, &branch));
        QCOMPARE(remote, QStringLiteral("github"));
        QCOMPARE(branch, QStringLiteral("main"));

        QVERIFY(gitrefs::splitRemoteRef(QStringLiteral("origin/feature/deep/name"), &remote, &branch));
        QCOMPARE(remote, QStringLiteral("origin"));
        QCOMPARE(branch, QStringLiteral("feature/deep/name"));

        // A remote name is validated by its own rules, not here: a dash or a
        // dot is ordinary, only the slash is impossible.
        QVERIFY(gitrefs::splitRemoteRef(QStringLiteral("my-remote-2/main"), &remote, &branch));
        QCOMPARE(remote, QStringLiteral("my-remote-2"));
        QCOMPARE(branch, QStringLiteral("main"));
    }

    void refusesNamesWithNoRemoteAndBranch()
    {
        QString remote = QStringLiteral("untouched");
        QString branch = QStringLiteral("untouched");
        for (const auto &name : { QStringLiteral("main"), QStringLiteral("/main"),
                                  QStringLiteral("github/"), QString(), QStringLiteral("/") }) {
            QVERIFY2(!gitrefs::splitRemoteRef(name, &remote, &branch),
                     qPrintable(QStringLiteral("accepted %1").arg(name)));
        }
        QCOMPARE(remote, QStringLiteral("untouched"));
        QCOMPARE(branch, QStringLiteral("untouched"));

        // A caller that only wants one half may pass null for the other.
        QVERIFY(gitrefs::splitRemoteRef(QStringLiteral("github/main"), nullptr, &branch));
        QCOMPARE(branch, QStringLiteral("main"));
    }

    void readsTheSlugOutOfAGitHubRemoteUrl()
    {
        QString slug;
        // Everything git accepts for a GitHub remote, with and without the
        // ".git" suffix and a trailing slash.
        for (const auto &url : { QStringLiteral("https://github.com/blackopsrepl/gitnaga.git"),
                                 QStringLiteral("https://github.com/blackopsrepl/gitnaga"),
                                 QStringLiteral("https://github.com/blackopsrepl/gitnaga/"),
                                 QStringLiteral("git@github.com:blackopsrepl/gitnaga.git"),
                                 QStringLiteral("ssh://git@github.com/blackopsrepl/gitnaga"),
                                 QStringLiteral("git://github.com/blackopsrepl/gitnaga.git"),
                                 QStringLiteral("github.com/blackopsrepl/gitnaga.git") }) {
            QVERIFY2(gitrefs::githubSlug(url, &slug), qPrintable(QStringLiteral("refused %1").arg(url)));
            QCOMPARE(slug, QStringLiteral("blackopsrepl/gitnaga"));
        }
    }

    void refusesRemotesThatAreNotAGitHubRepository()
    {
        QString slug = QStringLiteral("untouched");
        // A lookalike host, another forge, a local path, and a URL that points
        // somewhere inside a repository rather than at it.
        for (const auto &url : { QStringLiteral("http://vigilance:3002/blackopsrepl/gitnaga.git"),
                                 QStringLiteral("/srv/lab/tools/gitnaga"),
                                 QStringLiteral("https://github.com.evil.example/o/r.git"),
                                 QStringLiteral("https://github.com/blackopsrepl/gitnaga/pulls"),
                                 QStringLiteral("https://github.com/blackopsrepl"),
                                 QStringLiteral("https://github.com/"),
                                 QString() }) {
            QVERIFY2(!gitrefs::githubSlug(url, &slug), qPrintable(QStringLiteral("accepted %1").arg(url)));
        }
        QCOMPARE(slug, QStringLiteral("untouched"));

        // A remote URL is not a browser URL: "owner/repo/extra" is refused
        // rather than silently read as "owner/repo".
        QVERIFY(!gitrefs::githubSlug(QStringLiteral("https://github.com/blackopsrepl/gitnaga/tree/main"), nullptr));
    }
};

QTEST_GUILESS_MAIN(GitRefsTest)

#include "git_refs_test.moc"
