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
    static GitResult<CommitInspection> inspectCommit(const QString &worktree, const QString &oid);
    static GitResult<QVector<DiffLine>> loadDiff(const QString &worktree, const QString &oid, const QString &path,
                                                 const QString &oldPath = {});
    static GitResult<QString> mutate(const QString &worktree, const QStringList &arguments, const QString &operation);

private:
    static GitResult<QByteArray> run(const QString &workingDirectory, const QStringList &arguments,
                                     const QString &operation, const QHash<QString, QString> &extraEnvironment = {});
    // Materializes the worktree as an ephemeral commit whose parent is HEAD and
    // returns its oid, or an empty string when the worktree matches HEAD.
    static GitResult<QString> createWorktreeCommit(const QString &worktree, const QString &gitDirectory,
                                                   const QString &headOid, const QString &authorName,
                                                   const QString &authorEmail);
};

} // namespace GitNaga
