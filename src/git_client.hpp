#pragma once

#include "domain.hpp"

#include <QByteArray>
#include <QHash>
#include <QStringList>

#include <expected>

namespace GitNaga {

struct GitError {
    QString operation;
    QString message;
    int exitCode = -1;
};

template<typename T>
using GitResult = std::expected<T, GitError>;

class GitClient final
{
public:
    static GitResult<RepositorySnapshot> loadRepository(const QString &path, int maximumCommits = 5000);
    static GitResult<QVector<Worktree>> listWorktrees(const QString &worktree);
    static GitResult<QString> addWorktree(const QString &repository, const QString &path,
                                          const QString &branch = {}, bool createBranch = false,
                                          bool detached = false, const QString &startPoint = {});
    static GitResult<QString> removeWorktree(const QString &repository, const QString &path, bool force = false);
    static GitResult<QString> lockWorktree(const QString &repository, const QString &path, const QString &reason = {});
    static GitResult<QString> unlockWorktree(const QString &repository, const QString &path);
    static GitResult<QString> moveWorktree(const QString &repository, const QString &path, const QString &newPath);
    static GitResult<QString> pruneWorktrees(const QString &repository);
    static GitResult<QString> repairWorktrees(const QString &repository);
    static GitResult<CommitInspection> inspectCommit(const QString &worktree, const QString &oid);
    static GitResult<CommitInspection> inspectWorktree(const QString &worktree);
    // The configured URL of one named remote, unmodified.
    static GitResult<QString> remoteUrl(const QString &worktree, const QString &remote);
    // The "owner/repository" slug behind one named remote, when that remote is
    // a GitHub repository. Empty optional when the remote is missing or is not
    // a GitHub URL.
    static std::optional<QString> githubRepository(const QString &worktree, const QString &remote);
    // Delete one branch on one remote through the configured gh CLI. The branch
    // is removed by name, so it works for a remote-tracking ref with no local
    // branch behind it.
    static GitResult<QString> deleteRemoteBranch(const QString &worktree, const QString &remote,
                                                 const QString &branch);
    static GitResult<QString> commitWorktree(const QString &worktree, const QStringList &include,
                                             const QStringList &unstage, const QString &summary,
                                             const QString &description);
    static GitResult<QVector<DiffLine>> loadDiff(const QString &worktree, const QString &oid, const QString &path,
                                                 const QString &oldPath = {});
    static GitResult<QString> mutate(const QString &worktree, const QStringList &arguments, const QString &operation);

private:
    // The one process runner: `program` is the executable and git is the
    // default. A caller that has to leave git says so explicitly.
    static GitResult<QByteArray> runCommand(const QString &workingDirectory, const QString &program,
                                            const QStringList &arguments, const QString &operation,
                                            const QHash<QString, QString> &extraEnvironment = {});
    static GitResult<QByteArray> run(const QString &workingDirectory, const QStringList &arguments,
                                     const QString &operation, const QHash<QString, QString> &extraEnvironment = {});
    // Materializes the worktree as an ephemeral commit whose parent is HEAD and
    // returns its oid, or an empty string when the worktree matches HEAD.
    static GitResult<QString> createWorktreeCommit(const QString &worktree, const QString &gitDirectory,
                                                   const QString &headOid, const QString &authorName,
                                                   const QString &authorEmail);
};

} // namespace GitNaga
