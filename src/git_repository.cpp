#include "git_client.hpp"

#include "git_parse.hpp"
#include "git_refs.hpp"

#include <QDir>

namespace GitNaga {

using detail::decode;

GitResult<RepositoryLocation> GitClient::locate(const QString &path)
{
    auto discovery = run(path,
                         { QStringLiteral("rev-parse"), QStringLiteral("--path-format=absolute"),
                           QStringLiteral("--git-dir"), QStringLiteral("--git-common-dir"),
                           QStringLiteral("--is-bare-repository") },
                         QStringLiteral("discover repository"));
    if (!discovery)
        return std::unexpected(discovery.error());

    const auto fields = discovery->split('\n');
    if (fields.size() < 3)
        return std::unexpected(GitError{ QStringLiteral("discover repository"), QStringLiteral("Unexpected rev-parse output") });

    RepositoryLocation location;
    location.gitDirectory = QDir::cleanPath(decode(fields.at(0)));
    location.commonDirectory = QDir::cleanPath(decode(fields.at(1)));
    location.isBare = decode(fields.at(2)).trimmed() == QStringLiteral("true");
    if (location.isBare) {
        location.worktree = QDir(path).absolutePath();
        return location;
    }
    auto toplevel = run(path,
                        { QStringLiteral("rev-parse"), QStringLiteral("--path-format=absolute"),
                          QStringLiteral("--show-toplevel") },
                        QStringLiteral("discover worktree"));
    if (!toplevel)
        return std::unexpected(toplevel.error());
    location.worktree = QDir::cleanPath(decode(*toplevel).trimmed());
    return location;
}

GitResult<RefSnapshot> GitClient::loadRefs(const RepositoryLocation &location)
{
    RefSnapshot snapshot;
    const auto &worktree = location.worktree;

    // An unborn branch has no HEAD commit and a detached HEAD no symbolic
    // ref; both answer with a non-zero exit that is an answer, not an error.
    RunOptions optional;
    optional.acceptedExitCodes = { 0, 1, 128 };
    auto head = run(worktree, { QStringLiteral("rev-parse"), QStringLiteral("-q"), QStringLiteral("--verify"),
                                QStringLiteral("HEAD^{commit}") },
                    QStringLiteral("read HEAD"), optional);
    if (!head)
        return std::unexpected(head.error());
    snapshot.headOid = decode(*head).trimmed();

    auto symbolic = run(worktree, { QStringLiteral("symbolic-ref"), QStringLiteral("-q"), QStringLiteral("HEAD") },
                        QStringLiteral("read HEAD"), optional);
    if (!symbolic)
        return std::unexpected(symbolic.error());
    const auto headRef = decode(*symbolic).trimmed();
    snapshot.detached = headRef.isEmpty();
    if (headRef.startsWith(QStringLiteral("refs/heads/")))
        snapshot.currentBranch = headRef.mid(11);

    auto refs = run(worktree, { QStringLiteral("for-each-ref"), gitrefs::refFormat() }, QStringLiteral("read refs"));
    if (!refs)
        return std::unexpected(refs.error());
    snapshot.refs = gitrefs::parseRefs(*refs);
    bool hasStash = false;
    for (auto &ref : snapshot.refs) {
        ref.isHead = ref.fullName == headRef;
        hasStash = hasStash || ref.kind == RefKind::Stash;
    }

    QByteArray stashOutput;
    if (hasStash) {
        auto stashes = run(worktree, { QStringLiteral("stash"), QStringLiteral("list"),
                                       QStringLiteral("--format=%H%x1f%gd%x1f%gs") },
                           QStringLiteral("list stashes"));
        if (stashes) {
            stashOutput = *stashes;
            snapshot.stashes = gitrefs::parseStashes(stashOutput);
        }
    }

    snapshot.fingerprint = *refs + '\0' + headRef.toUtf8() + '\0' + snapshot.headOid.toUtf8() + '\0' + stashOutput;
    return snapshot;
}

GitResult<RepositorySnapshot> GitClient::loadRepository(const QString &path, const HistoryRequest &request)
{
    RepositorySnapshot snapshot;
    auto location = locate(path);
    if (!location)
        return std::unexpected(location.error());
    snapshot.location = *location;

    auto worktrees = listWorktrees(snapshot.location.worktree);
    if (!worktrees)
        return std::unexpected(worktrees.error());
    snapshot.worktrees = *worktrees;

    auto refs = loadRefs(snapshot.location);
    if (!refs)
        return std::unexpected(refs.error());
    snapshot.refs = *refs;

    auto status = loadStatus(snapshot.location);
    if (!status)
        return std::unexpected(status.error());
    snapshot.status = *status;

    auto history = streamHistory(snapshot.location.worktree, historyRevisions(snapshot.refs, request),
                                 gitrefs::decorations(snapshot.refs.refs, request.showInternal, request.hidden), 5000,
                                 [&snapshot](QVector<Commit> &&chunk) { snapshot.commits.append(std::move(chunk)); });
    if (!history)
        return std::unexpected(history.error());
    return snapshot;
}

} // namespace GitNaga
