#pragma once

#include "git_client.hpp"
#include "rebase_todo.hpp"

namespace GitNaga::gitrebase {

// The commits an interactive rebase from one commit rewrites.
struct Range {
    // Where the rewritten commits are replayed on; empty for --root.
    QString base;
    // Oldest first, the order git replays them in.
    QVector<rebase::Step> steps;
};

// From `oid` (inclusive) to HEAD. Refuses a commit HEAD does not contain and
// a range with merges, which a linear todo would silently flatten.
GitResult<Range> loadRange(const QString &worktree, const QString &oid);

// Runs `git rebase -i` with the todo built from `steps`; no editor ever
// opens. A conflict or an `edit` leaves the rebase stopped in the worktree,
// where the in-progress operation takes over.
GitResult<QString> run(const QString &worktree, const QString &base, const QVector<rebase::Step> &steps,
                       bool autostash, const RunOptions &options = {});

// The quick actions, each one rebase over the commits above the target.
GitResult<QString> reword(const QString &worktree, const QString &oid, const QString &message,
                          const RunOptions &options = {});
GitResult<QString> squashIntoParent(const QString &worktree, const QString &oid, const RunOptions &options = {});
GitResult<QString> drop(const QString &worktree, const QString &oid, const RunOptions &options = {});

} // namespace GitNaga::gitrebase
