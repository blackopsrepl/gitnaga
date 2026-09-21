#pragma once

#include "domain.hpp"

#include <QByteArray>
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

QString workSummaryText(const WorktreeState &state);

class GitClient final
{
public:
    static GitResult<RepositorySnapshot> loadRepository(const QString &path, int maximumCommits = 5000);
    static GitResult<WorktreeState> readStatus(const QString &worktree);
    static GitResult<CommitInspection> inspectCommit(const QString &worktree, const QString &oid);
    static GitResult<CommitInspection> inspectWorktree(const QString &worktree);
    static GitResult<QVector<DiffLine>> loadDiff(const QString &worktree, const QString &oid, const QString &path);
    static GitResult<QVector<DiffLine>> loadWorktreeDiff(const QString &worktree, const QString &path, const QString &oldPath = {});
    static GitResult<QString> mutate(const QString &worktree, const QStringList &arguments, const QString &operation);

private:
    static GitResult<QByteArray> run(const QString &workingDirectory, const QStringList &arguments, const QString &operation);
    static GitResult<QByteArray> runAllowingDiffExit(const QString &workingDirectory, const QStringList &arguments, const QString &operation);
    static GitResult<QByteArray> runImpl(const QString &workingDirectory, const QStringList &arguments, const QString &operation, bool acceptExitOne);
};

} // namespace GitNaga
