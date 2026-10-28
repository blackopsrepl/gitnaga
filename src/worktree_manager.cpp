#include "worktree_manager.hpp"

#include "repository_controller.hpp"

#include <QDir>
#include <QFileInfo>
#include <QVariantMap>

namespace GitNaga {
namespace {

const Worktree *registeredWorktree(const RepositorySnapshot &repository, const QString &path)
{
    const auto requested = QDir::cleanPath(QDir(path).absolutePath());
    for (const auto &worktree : repository.worktrees) {
        if (QDir::cleanPath(worktree.path) == requested)
            return &worktree;
    }
    return nullptr;
}

} // namespace

WorktreeManager::WorktreeManager(RepositoryController *repository)
    : QObject(repository)
    , m_repository(repository)
{
}

void WorktreeManager::openWorktree(const QString &path)
{
    const auto *target = registeredWorktree(m_repository->repository(), path);
    if (!target || target->isPrunable || !QFileInfo(target->path).isDir()) {
        const auto message = tr("Worktree is no longer registered");
        m_repository->reportFailure(message);
        return;
    }
    m_repository->openRepositoryPath(target->path);
}

void WorktreeManager::addWorktree(const QString &path, const QString &branch, const QString &mode,
                                       const QString &startPoint)
{
    if (path.trimmed().isEmpty())
        return;
    QStringList arguments{ QStringLiteral("worktree"), QStringLiteral("add") };
    if (mode == QStringLiteral("new")) {
        if (branch.trimmed().isEmpty())
            return;
        arguments << QStringLiteral("-b") << branch;
    } else if (mode == QStringLiteral("existing")) {
        if (branch.trimmed().isEmpty() || !startPoint.isEmpty())
            return;
    } else if (mode == QStringLiteral("detached")) {
        arguments.append(QStringLiteral("--detach"));
    } else {
        return;
    }
    arguments << QStringLiteral("--") << path;
    if (mode == QStringLiteral("existing"))
        arguments.append(branch);
    else if (!startPoint.isEmpty())
        arguments.append(startPoint);
    m_repository->runOperation(QStringLiteral("add worktree"), arguments, tr("Added worktree at %1").arg(path));
}

void WorktreeManager::removeWorktree(const QString &path, bool force)
{
    const auto *target = registeredWorktree(m_repository->repository(), path);
    if (!target || target->isMain || target->isBare || target->isPrunable || target->path == m_repository->repositoryPath()) {
        const auto message = tr("Only an inactive linked worktree can be removed");
        m_repository->reportFailure(message);
        return;
    }
    QStringList arguments{ QStringLiteral("worktree"), QStringLiteral("remove") };
    if (force)
        arguments.append(QStringLiteral("--force"));
    arguments << QStringLiteral("--") << target->path;
    m_repository->runOperation(QStringLiteral("remove worktree"), arguments, tr("Removed worktree at %1").arg(target->path));
}

void WorktreeManager::lockWorktree(const QString &path, const QString &reason)
{
    const auto *target = registeredWorktree(m_repository->repository(), path);
    if (!target || target->isMain || target->isBare || target->isPrunable)
        return;
    QStringList arguments{ QStringLiteral("worktree"), QStringLiteral("lock") };
    if (!reason.isEmpty())
        arguments << QStringLiteral("--reason") << reason;
    arguments << QStringLiteral("--") << target->path;
    m_repository->runOperation(QStringLiteral("lock worktree"), arguments, tr("Locked worktree at %1").arg(target->path));
}

void WorktreeManager::unlockWorktree(const QString &path)
{
    const auto *target = registeredWorktree(m_repository->repository(), path);
    if (!target || target->isMain || target->isBare || target->isPrunable)
        return;
    m_repository->runOperation(QStringLiteral("unlock worktree"),
                 { QStringLiteral("worktree"), QStringLiteral("unlock"), QStringLiteral("--"), target->path },
                 tr("Unlocked worktree at %1").arg(target->path));
}

void WorktreeManager::moveWorktree(const QString &path, const QString &newPath)
{
    const auto *target = registeredWorktree(m_repository->repository(), path);
    if (!target || target->isMain || target->isBare || target->isPrunable || target->path == m_repository->repositoryPath() || newPath.trimmed().isEmpty())
        return;
    m_repository->runOperation(QStringLiteral("move worktree"),
                 { QStringLiteral("worktree"), QStringLiteral("move"), QStringLiteral("--"), target->path, newPath },
                 tr("Moved worktree to %1").arg(newPath));
}

void WorktreeManager::pruneWorktrees()
{
    m_repository->runOperation(QStringLiteral("prune worktrees"),
                 { QStringLiteral("worktree"), QStringLiteral("prune") }, tr("Pruned stale worktree metadata"));
}

void WorktreeManager::repairWorktrees()
{
    m_repository->runOperation(QStringLiteral("repair worktrees"),
                 { QStringLiteral("worktree"), QStringLiteral("repair") }, tr("Repaired worktree links"));
}

} // namespace GitNaga
