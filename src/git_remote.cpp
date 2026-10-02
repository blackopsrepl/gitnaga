#include "git_client.hpp"

#include "git_refs.hpp"

#include <QProcess>

#include <optional>

namespace GitNaga {
namespace {

GitError remoteError(const QString &operation, const QString &message)
{
    return GitError{ operation, message };
}

} // namespace

GitResult<QString> GitClient::remoteUrl(const QString &worktree, const QString &remote)
{
    const auto operation = QStringLiteral("read remote url");
    if (remote.trimmed().isEmpty())
        return std::unexpected(GitError{ operation, QStringLiteral("Remote name is empty") });
    return mutate(worktree, { QStringLiteral("remote"), QStringLiteral("get-url"), remote }, operation);
}

// Deleting a remote branch is a forge operation, not a git one: a local
// repository has no way to remove a ref on the remote it tracks. The backend is
// the configured gh CLI and nothing else — no API client, no credential
// handling, no second forge — so the credentials, the host, and the behaviour
// are exactly what `gh` already has on this machine.

std::optional<QString> GitClient::githubRepository(const QString &worktree, const QString &remote)
{
    const auto url = remoteUrl(worktree, remote);
    if (!url)
        return std::nullopt;
    QString slug;
    if (!gitrefs::githubSlug(*url, &slug))
        return std::nullopt;
    return slug;
}

GitResult<QString> GitClient::deleteRemoteBranch(const QString &worktree, const QString &remote,
                                                 const QString &branch)
{
    const auto operation = QStringLiteral("delete remote branch");
    if (remote.trimmed().isEmpty())
        return std::unexpected(remoteError(operation, QStringLiteral("Remote name is empty")));
    if (branch.trimmed().isEmpty())
        return std::unexpected(remoteError(operation, QStringLiteral("Branch name is empty")));

    const auto slug = githubRepository(worktree, remote);
    if (!slug) {
        return std::unexpected(remoteError(
            operation, QStringLiteral("Remote %1 is not a GitHub repository").arg(remote)));
    }

    // The ref path is passed verbatim, so a branch that exists only as a
    // remote-tracking ref is deleted by name rather than by local commit.
    // Arguments are handed to gh as a list: no shell, nothing to quote.
    auto result = runCommand(worktree, QStringLiteral("gh"),
                             { QStringLiteral("api"), QStringLiteral("--method"), QStringLiteral("DELETE"),
                               QStringLiteral("repos/%1/git/refs/heads/%2").arg(*slug, branch) },
                             operation);
    if (result)
        return QString();
    if (result.error().exitCode == QProcess::FailedToStart) {
        result.error().message = QStringLiteral("gh is not installed or not on PATH");
        return std::unexpected(result.error());
    }
    // gh prints the API's own message, and an unauthenticated gh prints how to
    // authenticate. Either is a better answer than the exit code alone.
    if (result.error().message.isEmpty())
        result.error().message = QStringLiteral("gh failed without a message");
    return std::unexpected(result.error());
}

} // namespace GitNaga
