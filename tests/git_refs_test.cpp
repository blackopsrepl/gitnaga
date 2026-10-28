#include "git_client.hpp"
#include "git_parse.hpp"
#include "git_refs.hpp"

#include <QTest>

// Naming contracts for remote-tracking refs, kept out of the client test so
// each file owns one concern and stays well inside the source-size cap.

using namespace GitNaga;

// Git's output is NUL-separated; a plain literal would stop at the first NUL.
template<qsizetype N>
QByteArray bytes(const char (&text)[N])
{
    return QByteArray(text, N - 1);
}

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

    void joinsTheRemoteAndBranchBack()
    {
        QCOMPARE(gitrefs::joinRemoteRef(QStringLiteral("github"), QStringLiteral("main")),
                 QStringLiteral("github/main"));
        QCOMPARE(gitrefs::joinRemoteRef(QStringLiteral("origin"), QStringLiteral("feature/deep")),
                 QStringLiteral("origin/feature/deep"));
    }

    void classifiesWhatAPersonBrowsesAndWhatIsInternal()
    {
        QCOMPARE(gitrefs::classify(QStringLiteral("refs/heads/feature/x")).kind, RefKind::Local);
        QCOMPARE(gitrefs::classify(QStringLiteral("refs/heads/feature/x")).name, QStringLiteral("feature/x"));
        const auto remote = gitrefs::classify(QStringLiteral("refs/remotes/origin/feature/x"));
        QCOMPARE(remote.kind, RefKind::Remote);
        QCOMPARE(remote.remote, QStringLiteral("origin"));
        QCOMPARE(remote.branch, QStringLiteral("feature/x"));
        QVERIFY(!remote.internal);
        QVERIFY(gitrefs::classify(QStringLiteral("refs/remotes/origin/HEAD")).internal);
        QVERIFY(gitrefs::classify(QStringLiteral("refs/stash")).internal);
        for (const auto &name : { QStringLiteral("refs/codex/snapshots/abc"), QStringLiteral("refs/original/refs/heads/main"),
                                  QStringLiteral("refs/notes/commits"), QStringLiteral("refs/prefetch/remotes/origin/main") }) {
            const auto ref = gitrefs::classify(name);
            QCOMPARE(ref.kind, RefKind::Other);
            QVERIFY2(ref.internal, qPrintable(name));
        }
        QVERIFY(!gitrefs::classify(QStringLiteral("refs/tags/v1.0")).internal);
    }

    void peelsAnnotatedTagsAndDropsNonCommits()
    {
        const QByteArray output = bytes(
            "refs/tags/v1\0" "aaaa\0tag\0" "cccc\0commit\0\0\n"
            "refs/tags/light\0" "cccc\0commit\0\0\0\0\n"
            "refs/codex/tree\0" "dddd\0tree\0\0\0\0\n"
            "refs/heads/main\0" "cccc\0commit\0\0\0refs/remotes/origin/main\0ahead 2, behind 1\n"
            "refs/heads/old\0" "eeee\0commit\0\0\0refs/remotes/origin/old\0gone\n");
        const auto refs = gitrefs::parseRefs(output);
        QCOMPARE(refs.size(), 5);
        QCOMPARE(refs.at(0).target, QStringLiteral("cccc"));
        QCOMPARE(refs.at(1).target, QStringLiteral("cccc"));
        QVERIFY(refs.at(2).target.isEmpty());
        QCOMPARE(refs.at(3).upstream, QStringLiteral("origin/main"));
        QCOMPARE(refs.at(3).ahead, 2);
        QCOMPARE(refs.at(3).behind, 1);
        QVERIFY(refs.at(4).upstreamGone);
    }

    void walksOnlyTheRefsAPersonAskedFor()
    {
        RefSnapshot snapshot;
        snapshot.headOid = QStringLiteral("cccc");
        snapshot.refs = gitrefs::parseRefs(bytes(
            "refs/heads/main\0cccc\0commit\0\0\0\0\n"
            "refs/heads/topic\0bbbb\0commit\0\0\0\0\n"
            "refs/remotes/origin/HEAD\0cccc\0commit\0\0\0\0\n"
            "refs/stash\0ffff\0commit\0\0\0\0\n"
            "refs/codex/x\0dddd\0commit\0\0\0\0\n"));
        HistoryRequest request;
        QCOMPARE(GitClient::historyRevisions(snapshot, request),
                 QStringList({ QStringLiteral("refs/heads/main"), QStringLiteral("refs/heads/topic"), QStringLiteral("HEAD") }));
        request.hidden.insert(QStringLiteral("refs/heads/topic"));
        QCOMPARE(GitClient::historyRevisions(snapshot, request),
                 QStringList({ QStringLiteral("refs/heads/main"), QStringLiteral("HEAD") }));
        request.showInternal = true;
        QVERIFY(GitClient::historyRevisions(snapshot, request).contains(QStringLiteral("refs/codex/x")));
        QVERIFY(!GitClient::historyRevisions(snapshot, request).contains(QStringLiteral("refs/stash")));
        request.solo = QStringLiteral("refs/heads/topic");
        QCOMPARE(GitClient::historyRevisions(snapshot, request),
                 QStringList({ QStringLiteral("refs/heads/topic"), QStringLiteral("HEAD") }));
    }

    void parsesPorcelainTwoStatus()
    {
        const QByteArray output = bytes(
            "# branch.oid cccc\0# branch.head main\0# branch.upstream origin/main\0# branch.ab +3 -1\0"
            "1 MM N... 100644 100644 100644 aaaa bbbb both file.txt\0"
            "2 R. N... 100644 100644 100644 aaaa aaaa R100 new name.txt\0old name.txt\0"
            "u UU N... 100644 100644 100644 100644 aaaa bbbb cccc conflict.txt\0"
            "1 .M S.M. 160000 160000 160000 aaaa aaaa lib\0"
            "? fresh.txt\0");
        const auto status = detail::parseStatus(output);
        QCOMPARE(status.upstream, QStringLiteral("origin/main"));
        QCOMPARE(status.ahead, 3);
        QCOMPARE(status.behind, 1);
        QCOMPARE(status.files.size(), 5);
        QCOMPARE(status.files.at(0).path, QStringLiteral("both file.txt"));
        QVERIFY(status.files.at(0).staged && status.files.at(0).unstaged);
        QCOMPARE(status.files.at(1).status, QStringLiteral("R"));
        QCOMPARE(status.files.at(1).path, QStringLiteral("new name.txt"));
        QCOMPARE(status.files.at(1).oldPath, QStringLiteral("old name.txt"));
        QVERIFY(status.files.at(2).conflicted);
        QVERIFY(status.files.at(3).submodule);
        QVERIFY(status.files.at(4).untracked);
    }
};

QTEST_GUILESS_MAIN(GitRefsTest)

#include "git_refs_test.moc"
