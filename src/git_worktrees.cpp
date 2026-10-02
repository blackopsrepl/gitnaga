#include "git_client.hpp"

#include <QDir>

namespace GitNaga {
namespace {

GitResult<QString> worktreeCommand(const QString &repository, const QStringList &arguments, const QString &operation)
{
    QStringList command{ QStringLiteral("worktree") };
    command.append(arguments);
    return GitClient::mutate(repository, command, operation);
}

GitError invalidWorktreeArgument(const QString &operation, const QString &message)
{
    return GitError{ operation, message };
}

} // namespace

GitResult<QVector<Worktree>> GitClient::listWorktrees(const QString &worktree)
{
    auto output = worktreeCommand(worktree,
                                  { QStringLiteral("list"), QStringLiteral("--porcelain"), QStringLiteral("-z") },
                                  QStringLiteral("list worktrees"));
    if (!output)
        return std::unexpected(output.error());

    QVector<Worktree> worktrees;
    Worktree *current = nullptr;
    for (const auto &field : output->split(QChar::Null, Qt::KeepEmptyParts)) {
        if (field.startsWith(QStringLiteral("worktree "))) {
            Worktree item;
            item.path = field.mid(9);
            worktrees.append(std::move(item));
            current = &worktrees.last();
            continue;
        }
        if (field.isEmpty() || !current)
            continue;
        const auto separator = field.indexOf(QLatin1Char(' '));
        const auto key = separator < 0 ? field : field.left(separator);
        const auto value = separator < 0 ? QString() : field.mid(separator + 1);
        if (key == QStringLiteral("HEAD"))
            current->head = value;
        else if (key == QStringLiteral("branch"))
            current->branch = value.startsWith(QStringLiteral("refs/heads/")) ? value.mid(11) : value;
        else if (key == QStringLiteral("bare"))
            current->isBare = true;
        else if (key == QStringLiteral("detached"))
            current->isDetached = true;
        else if (key == QStringLiteral("locked")) {
            current->isLocked = true;
            current->lockReason = value;
        } else if (key == QStringLiteral("prunable")) {
            current->isPrunable = true;
            current->pruneReason = value;
        }
    }
    if (!worktrees.isEmpty())
        worktrees.first().isMain = true;
    return worktrees;
}

GitResult<QString> GitClient::addWorktree(const QString &repository, const QString &path,
                                          const QString &branch, bool createBranch,
                                          bool detached, const QString &startPoint)
{
    const QString operation = QStringLiteral("add worktree");
    if (path.trimmed().isEmpty())
        return std::unexpected(invalidWorktreeArgument(operation, QStringLiteral("Worktree path is empty")));
    if (createBranch && branch.trimmed().isEmpty())
        return std::unexpected(invalidWorktreeArgument(operation, QStringLiteral("Branch name is empty")));
    if (detached && createBranch)
        return std::unexpected(invalidWorktreeArgument(operation, QStringLiteral("Detached worktrees cannot create a branch")));
    if (!createBranch && !detached && !branch.isEmpty() && !startPoint.isEmpty())
        return std::unexpected(invalidWorktreeArgument(operation, QStringLiteral("A start point cannot be combined with an existing branch")));

    QStringList arguments{ QStringLiteral("add") };
    if (detached)
        arguments.append(QStringLiteral("--detach"));
    else if (createBranch)
        arguments << QStringLiteral("-b") << branch;
    arguments << QStringLiteral("--") << path;
    if (!startPoint.isEmpty())
        arguments.append(startPoint);
    else if (!createBranch && !detached && !branch.isEmpty())
        arguments.append(branch);
    return worktreeCommand(repository, arguments, operation);
}

GitResult<QString> GitClient::removeWorktree(const QString &repository, const QString &path, bool force)
{
    if (path.trimmed().isEmpty())
        return std::unexpected(invalidWorktreeArgument(QStringLiteral("remove worktree"), QStringLiteral("Worktree path is empty")));
    QStringList arguments{ QStringLiteral("remove") };
    if (force)
        arguments.append(QStringLiteral("--force"));
    arguments << QStringLiteral("--") << path;
    return worktreeCommand(repository, arguments, QStringLiteral("remove worktree"));
}

GitResult<QString> GitClient::lockWorktree(const QString &repository, const QString &path, const QString &reason)
{
    if (path.trimmed().isEmpty())
        return std::unexpected(invalidWorktreeArgument(QStringLiteral("lock worktree"), QStringLiteral("Worktree path is empty")));
    QStringList arguments{ QStringLiteral("lock") };
    if (!reason.isEmpty())
        arguments << QStringLiteral("--reason") << reason;
    arguments << QStringLiteral("--") << path;
    return worktreeCommand(repository, arguments, QStringLiteral("lock worktree"));
}

GitResult<QString> GitClient::unlockWorktree(const QString &repository, const QString &path)
{
    if (path.trimmed().isEmpty())
        return std::unexpected(invalidWorktreeArgument(QStringLiteral("unlock worktree"), QStringLiteral("Worktree path is empty")));
    return worktreeCommand(repository, { QStringLiteral("unlock"), QStringLiteral("--"), path },
                           QStringLiteral("unlock worktree"));
}

GitResult<QString> GitClient::moveWorktree(const QString &repository, const QString &path, const QString &newPath)
{
    if (path.trimmed().isEmpty() || newPath.trimmed().isEmpty())
        return std::unexpected(invalidWorktreeArgument(QStringLiteral("move worktree"), QStringLiteral("Worktree path is empty")));
    return worktreeCommand(repository, { QStringLiteral("move"), QStringLiteral("--"), path, newPath },
                           QStringLiteral("move worktree"));
}

GitResult<QString> GitClient::pruneWorktrees(const QString &repository)
{
    return worktreeCommand(repository, { QStringLiteral("prune") }, QStringLiteral("prune worktrees"));
}

GitResult<QString> GitClient::repairWorktrees(const QString &repository)
{
    return worktreeCommand(repository, { QStringLiteral("repair") }, QStringLiteral("repair worktrees"));
}

} // namespace GitNaga
