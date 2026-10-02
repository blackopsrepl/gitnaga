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

QVariantList RepositoryController::worktrees() const
{
    QVariantList entries;
    for (const auto &worktree : m_repository.worktrees) {
        QVariantMap entry;
        entry.insert(QStringLiteral("path"), worktree.path);
        entry.insert(QStringLiteral("head"), worktree.head);
        entry.insert(QStringLiteral("branch"), worktree.branch);
        entry.insert(QStringLiteral("lockReason"), worktree.lockReason);
        entry.insert(QStringLiteral("pruneReason"), worktree.pruneReason);
        entry.insert(QStringLiteral("isMain"), worktree.isMain);
        entry.insert(QStringLiteral("isBare"), worktree.isBare);
        entry.insert(QStringLiteral("isDetached"), worktree.isDetached);
        entry.insert(QStringLiteral("isLocked"), worktree.isLocked);
        entry.insert(QStringLiteral("isPrunable"), worktree.isPrunable);
        entry.insert(QStringLiteral("isCurrent"), worktree.path == m_repository.worktree);
        entries.append(entry);
    }
    return entries;
}

void RepositoryController::openWorktree(const QString &path)
{
    const auto *target = registeredWorktree(m_repository, path);
    if (!target || target->isPrunable || !QFileInfo(target->path).isDir()) {
        const auto message = tr("Worktree is no longer registered");
        setOperationMessage(message);
        emit operationFinished(false, message);
        return;
    }
    openRepositoryPath(target->path);
}

void RepositoryController::addWorktree(const QString &path, const QString &branch, const QString &mode,
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
    runOperation(QStringLiteral("add worktree"), arguments, tr("Added worktree at %1").arg(path));
}

void RepositoryController::removeWorktree(const QString &path, bool force)
{
    const auto *target = registeredWorktree(m_repository, path);
    if (!target || target->isMain || target->isBare || target->isPrunable || target->path == m_repository.worktree) {
        const auto message = tr("Only an inactive linked worktree can be removed");
        setOperationMessage(message);
        emit operationFinished(false, message);
        return;
    }
    QStringList arguments{ QStringLiteral("worktree"), QStringLiteral("remove") };
    if (force)
        arguments.append(QStringLiteral("--force"));
    arguments << QStringLiteral("--") << target->path;
    runOperation(QStringLiteral("remove worktree"), arguments, tr("Removed worktree at %1").arg(target->path));
}

void RepositoryController::lockWorktree(const QString &path, const QString &reason)
{
    const auto *target = registeredWorktree(m_repository, path);
    if (!target || target->isMain || target->isBare || target->isPrunable)
        return;
    QStringList arguments{ QStringLiteral("worktree"), QStringLiteral("lock") };
    if (!reason.isEmpty())
        arguments << QStringLiteral("--reason") << reason;
    arguments << QStringLiteral("--") << target->path;
    runOperation(QStringLiteral("lock worktree"), arguments, tr("Locked worktree at %1").arg(target->path));
}

void RepositoryController::unlockWorktree(const QString &path)
{
    const auto *target = registeredWorktree(m_repository, path);
    if (!target || target->isMain || target->isBare || target->isPrunable)
        return;
    runOperation(QStringLiteral("unlock worktree"),
                 { QStringLiteral("worktree"), QStringLiteral("unlock"), QStringLiteral("--"), target->path },
                 tr("Unlocked worktree at %1").arg(target->path));
}

void RepositoryController::moveWorktree(const QString &path, const QString &newPath)
{
    const auto *target = registeredWorktree(m_repository, path);
    if (!target || target->isMain || target->isBare || target->isPrunable || target->path == m_repository.worktree || newPath.trimmed().isEmpty())
        return;
    runOperation(QStringLiteral("move worktree"),
                 { QStringLiteral("worktree"), QStringLiteral("move"), QStringLiteral("--"), target->path, newPath },
                 tr("Moved worktree to %1").arg(newPath));
}

void RepositoryController::pruneWorktrees()
{
    runOperation(QStringLiteral("prune worktrees"),
                 { QStringLiteral("worktree"), QStringLiteral("prune") }, tr("Pruned stale worktree metadata"));
}

void RepositoryController::repairWorktrees()
{
    runOperation(QStringLiteral("repair worktrees"),
                 { QStringLiteral("worktree"), QStringLiteral("repair") }, tr("Repaired worktree links"));
}

} // namespace GitNaga
