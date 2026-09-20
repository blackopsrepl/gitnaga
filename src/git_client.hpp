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

class GitClient final
{
public:
    static GitResult<RepositorySnapshot> loadRepository(const QString &path, int maximumCommits = 5000);
    static GitResult<CommitInspection> inspectCommit(const QString &worktree, const QString &oid);
    static GitResult<QVector<DiffLine>> loadDiff(const QString &worktree, const QString &oid, const QString &path);

private:
    static GitResult<QByteArray> run(const QString &workingDirectory, const QStringList &arguments, const QString &operation);
};

} // namespace GitNaga
