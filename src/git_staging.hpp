#pragma once

#include "git_client.hpp"

namespace GitNaga::staging {

// Uncommitted work as two lists, the way git holds it: what the worktree
// changes on top of the index (including untracked and conflicted paths) and
// what the index changes on top of HEAD. A file edited after staging is in
// both. Unstaged entries describe the worktree side (no rename source).
void splitStatus(const QVector<FileChange> &files, QVector<FileChange> *unstaged, QVector<FileChange> *staged);

// File-level moves between the worktree, the index and HEAD.
GitResult<QString> stage(const QString &worktree, const QStringList &paths, const RunOptions &options = {});
GitResult<QString> unstage(const QString &worktree, const QStringList &paths, const RunOptions &options = {});
// Tracked paths go back to their staged content; untracked paths are deleted.
GitResult<QString> discard(const QString &worktree, const QStringList &tracked, const QStringList &untracked,
                           const RunOptions &options = {});

// Applies a patch from patch::build: to the index when `cached`, taking the
// lines back out when `reverse`.
GitResult<QString> applyPatch(const QString &worktree, const QByteArray &patch, bool cached, bool reverse,
                              const RunOptions &options = {});

// Commits the index as it stands; the new HEAD on success. Amending without
// staged changes rewrites only the message.
GitResult<QString> commit(const QString &worktree, const QString &summary, const QString &description, bool amend,
                          const RunOptions &options = {});

struct HeadMessage {
    QString summary;
    QString description;
    // HEAD is already on its upstream: amending it rewrites published work.
    bool published = false;
};
GitResult<HeadMessage> headMessage(const QString &worktree);

GitResult<QString> stashPush(const QString &worktree, const QString &message, bool includeUntracked,
                             const RunOptions &options = {});
// `action` is "apply", "pop" or "drop". The selector is checked against the
// oid first, so a stash list that moved meanwhile never loses the wrong one.
GitResult<QString> stashAction(const QString &worktree, const QString &action, const QString &selector,
                               const QString &oid, const RunOptions &options = {});

// A stash reviewed like a commit: its changes against the commit it was
// taken on, plus the untracked files it saved in its third parent (marked
// `untracked`, diffed from that parent).
GitResult<CommitInspection> inspectStash(const QString &worktree, const QString &oid);

} // namespace GitNaga::staging
