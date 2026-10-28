#pragma once

#include "domain.hpp"

#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QStringList>

#include <atomic>
#include <expected>
#include <functional>
#include <memory>

namespace GitNaga {

struct GitError {
    QString operation;
    QString message;
    int exitCode = -1;
};

template<typename T>
using GitResult = std::expected<T, GitError>;

// Shared between the GUI and a running git process: setting it stops the
// process at its next poll. A null token means the run cannot be cancelled.
using CancelToken = std::shared_ptr<std::atomic_bool>;

struct RunOptions {
    QHash<QString, QString> environment;
    // Exit codes that still count as success: `diff --no-index` exits 1 when
    // it found differences, which is the answer, not a failure.
    QList<int> acceptedExitCodes{ 0 };
    QByteArray input;
    CancelToken cancel;
    // Each progress line git writes to stderr (fetch, push, clone).
    std::function<void(const QString &)> progress;
    // Stdout as it arrives. When set, run() returns an empty buffer and the
    // caller owns the bytes; returning false stops the process.
    std::function<bool(const QByteArray &)> output;
};

struct HistoryRequest {
    bool showInternal = false;
    // Full ref names the user hid from the graph.
    QSet<QString> hidden;
    // When set, the graph shows only this ref's history (plus HEAD).
    QString solo;
};

class GitClient final
{
public:
    // Reading.
    static GitResult<RepositoryLocation> locate(const QString &path);
    static GitResult<RefSnapshot> loadRefs(const RepositoryLocation &location);
    static GitResult<WorktreeStatus> loadStatus(const RepositoryLocation &location);
    // Revisions the graph walks: the refs a person asked to see.
    static QStringList historyRevisions(const RefSnapshot &refs, const HistoryRequest &request);
    // Walks the history in topological order and hands it over in laid-out
    // chunks, so the first screen paints before a large history is read.
    static GitResult<qsizetype> streamHistory(const QString &worktree, const QStringList &revisions,
                                              const QHash<QString, QVector<Ref>> &decorations, int chunkSize,
                                              const std::function<void(QVector<Commit> &&chunk)> &onChunk,
                                              CancelToken cancel = {});
    // Everything at once; the application streams, tests use this.
    static GitResult<RepositorySnapshot> loadRepository(const QString &path, const HistoryRequest &request = {});
    static GitResult<QVector<Worktree>> listWorktrees(const QString &worktree);

    static GitResult<CommitInspection> inspectCommit(const QString &worktree, const QString &oid);
    static CommitInspection inspectWorktree(const WorktreeStatus &status);
    static GitResult<QVector<DiffLine>> loadDiff(const QString &worktree, const QString &oid, const QString &path,
                                                 const QString &oldPath = {}, DiffSide side = DiffSide::Commit,
                                                 bool untracked = false);

    // Writing.
    static GitResult<QString> addWorktree(const QString &repository, const QString &path,
                                          const QString &branch = {}, bool createBranch = false,
                                          bool detached = false, const QString &startPoint = {});
    static GitResult<QString> removeWorktree(const QString &repository, const QString &path, bool force = false);
    static GitResult<QString> lockWorktree(const QString &repository, const QString &path, const QString &reason = {});
    static GitResult<QString> unlockWorktree(const QString &repository, const QString &path);
    static GitResult<QString> moveWorktree(const QString &repository, const QString &path, const QString &newPath);
    static GitResult<QString> pruneWorktrees(const QString &repository);
    static GitResult<QString> repairWorktrees(const QString &repository);
    // Delete one branch on one remote over that remote's own transport, so any
    // git server works with the credentials the remote already has. The branch
    // is removed by name, so it works for a remote-tracking ref with no local
    // branch behind it; git prunes that tracking ref itself once the server
    // accepts the deletion.
    static GitResult<QString> deleteRemoteBranch(const QString &worktree, const QString &remote,
                                                 const QString &branch, const RunOptions &options = {});
    static GitResult<QString> mutate(const QString &worktree, const QStringList &arguments, const QString &operation,
                                     const RunOptions &options = {});

    // The one process runner every git command goes through. No timeout: a
    // pre-commit hook or a slow remote takes as long as it takes, and the
    // cancel token is how a person stops it.
    static GitResult<QByteArray> run(const QString &workingDirectory, const QStringList &arguments,
                                     const QString &operation, const RunOptions &options = {});
    static GitResult<QByteArray> run(const QString &workingDirectory, const QStringList &arguments,
                                     const QString &operation, const QHash<QString, QString> &environment);
};

} // namespace GitNaga
