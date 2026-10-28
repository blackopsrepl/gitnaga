#pragma once

#include "git_client.hpp"

#include <QString>
#include <QStringList>

// Talking to remotes: fetch, pull, push and upstream tracking. Every call goes
// through GitClient::run, so the queue, cancel token and footer progress work
// the same as for any other operation.
namespace GitNaga::gitsync {

enum class PullMode {
    FastForwardOrMerge, // `pull --no-rebase`: fast-forward, merge when diverged
    FastForwardOnly,
    Rebase,
};

PullMode pullModeFromName(const QString &name);
QString pullModeName(PullMode mode);

// The options a network operation needs on top of the caller's: GitNaga
// answers credential and passphrase prompts itself (see askpass.hpp), because
// GIT_TERMINAL_PROMPT=0 otherwise turns every prompt into a failure. A user who
// configured their own askpass keeps it.
RunOptions withCredentials(const QString &worktree, RunOptions options, const QString &askpassProgram);

GitResult<QStringList> listRemotes(const QString &worktree);
// Where a branch pushes to by its upstream configuration: the remote and the
// remote's branch name, which need not match the local one. Empty remote when
// the branch tracks nothing.
struct Tracking {
    QString remote;
    QString branch;
};
Tracking trackingOf(const QString &worktree, const QString &branch);
// The remote a branch with no upstream publishes to: the only one, or
// "origin". Empty when the choice is the user's to make.
QString defaultRemote(const QStringList &remotes);

GitResult<QString> fetchAll(const QString &worktree, const RunOptions &options = {});
GitResult<QString> pull(const QString &worktree, PullMode mode, const RunOptions &options = {});

struct PushRequest {
    QString remote;
    QString branch;
    // The branch name on the remote; the local name when empty.
    QString target;
    bool setUpstream = false;
    // `--force-with-lease`, never `--force`: the server's branch is replaced
    // only while it still points where this repository last saw it.
    bool forceWithLease = false;
};
GitResult<QString> push(const QString &worktree, const PushRequest &request, const RunOptions &options = {});
GitResult<QString> pushTag(const QString &worktree, const QString &remote, const QString &tag,
                           const RunOptions &options = {});

// A push the server refused because the branch moved there: the case a force
// with lease can resolve, as opposed to a hook or a permission refusal.
bool isNonFastForward(const QString &message);

GitResult<QString> setUpstream(const QString &worktree, const QString &branch, const QString &upstream);
GitResult<QString> unsetUpstream(const QString &worktree, const QString &branch);

} // namespace GitNaga::gitsync
