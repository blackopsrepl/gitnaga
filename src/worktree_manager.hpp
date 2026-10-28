#pragma once

#include <QObject>

namespace GitNaga {

class RepositoryController;

// Linked worktrees: open, add, remove, lock, move, prune and repair. QML
// reaches it as `repository.worktreeManager`; the list itself is
// `repository.worktrees`.
class WorktreeManager final : public QObject
{
    Q_OBJECT

public:
    explicit WorktreeManager(RepositoryController *repository);

    Q_INVOKABLE void openWorktree(const QString &path);
    // `mode` is "new" (create `branch`), "existing" or "detached".
    Q_INVOKABLE void addWorktree(const QString &path, const QString &branch, const QString &mode,
                                 const QString &startPoint);
    Q_INVOKABLE void removeWorktree(const QString &path, bool force);
    Q_INVOKABLE void lockWorktree(const QString &path, const QString &reason);
    Q_INVOKABLE void unlockWorktree(const QString &path);
    Q_INVOKABLE void moveWorktree(const QString &path, const QString &newPath);
    Q_INVOKABLE void pruneWorktrees();
    Q_INVOKABLE void repairWorktrees();

private:
    RepositoryController *m_repository;
};

} // namespace GitNaga
