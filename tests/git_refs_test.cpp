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
};

QTEST_GUILESS_MAIN(GitRefsTest)

#include "git_refs_test.moc"
