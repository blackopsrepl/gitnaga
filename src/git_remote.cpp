#include "git_client.hpp"

namespace GitNaga {
namespace {

GitError remoteError(const QString &operation, const QString &message)
{
    return GitError{ operation, message };
}

} // namespace

// Deleting a remote branch has to work against any git server, so it goes
// through the remote's own transport rather than a forge API: git push speaks
// https, ssh, git, and file, using whatever credentials are already configured
// for that remote, and needs no per-forge client. git prunes the local
// remote-tracking ref itself once the server accepts the deletion.

GitResult<QString> GitClient::deleteRemoteBranch(const QString &worktree, const QString &remote,
                                                 const QString &branch)
{
    const auto operation = QStringLiteral("delete remote branch");
    if (remote.trimmed().isEmpty())
        return std::unexpected(remoteError(operation, QStringLiteral("Remote name is empty")));
    if (branch.trimmed().isEmpty())
        return std::unexpected(remoteError(operation, QStringLiteral("Branch name is empty")));

    // The full refname, not the short branch name: git expands a full refname
    // on the remote, so a ref that appears only as a remote-tracking ref is
    // still resolved by the server, while a short name is resolved locally and
    // fails with "remote ref does not exist" for exactly that case.
    const auto ref = QStringLiteral("refs/heads/%1").arg(branch);
    auto result = run(worktree,
                      { QStringLiteral("push"), remote, QStringLiteral("--delete"), ref },
                      operation);
    if (result)
        return QString();
    // Surface git's own message: it names the cause (authentication, a server
    // that refuses deletions, a repository that moved) better than the exit
    // code can. git reports a ref that is already gone as a warning and exits
    // zero, so a non-zero exit here is a real refusal.
    if (result.error().message.isEmpty())
        result.error().message = QStringLiteral("git could not delete %1 on %2").arg(ref, remote);
    return std::unexpected(result.error());
}

} // namespace GitNaga
