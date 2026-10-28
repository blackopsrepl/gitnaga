#include "remote_sync.hpp"

#include "git_sync.hpp"
#include "repository_controller.hpp"
#include "undo_stack.hpp"

#include <memory>

namespace GitNaga {

void RemoteSync::pushTag(const QString &tag, const QString &chosenRemote)
{
    if (tag.isEmpty())
        return;
    auto choices = std::make_shared<QStringList>();
    m_repository->runAsyncOperation(tr("Pushed tag %1").arg(tag),
                                    [tag, chosenRemote, choices](const QString &worktree, const RunOptions &options) -> GitResult<QString> {
        auto remote = chosenRemote;
        if (remote.isEmpty()) {
            auto remotes = gitsync::listRemotes(worktree);
            if (!remotes)
                return std::unexpected(remotes.error());
            if (remotes->isEmpty())
                return std::unexpected(GitError{ QStringLiteral("push tag"), QObject::tr("This repository has no remote to push to") });
            remote = gitsync::defaultRemote(*remotes);
            if (remote.isEmpty()) {
                *choices = *remotes;
                return std::unexpected(GitError{ QStringLiteral("push tag"), QObject::tr("Choose the remote to push %1 to").arg(tag) });
            }
        }
        if (options.progress)
            options.progress(QObject::tr("Pushing tag %1 to %2…").arg(tag, remote));
        return gitsync::pushTag(worktree, remote, tag, gitsync::withCredentials(worktree, options, askpassProgram()));
    }, [this, tag, choices](const QString &worktree, const GitResult<QString> &result) {
        if (result)
            m_repository->undoStack()->block(tr("push tag %1").arg(tag), worktree);
        if (!choices->isEmpty())
            emit remoteChoiceNeeded(tag, *choices);
    });
}

// The command that puts a branch's upstream setting back as it is now.
QVector<QStringList> RemoteSync::restoreUpstream(const QString &branch) const
{
    for (const auto &ref : m_repository->repository().refs.refs)
        if (ref.kind == RefKind::Local && ref.name == branch && !ref.upstream.isEmpty())
            return { { QStringLiteral("branch"), QStringLiteral("--set-upstream-to=") + ref.upstream, branch } };
    return { { QStringLiteral("branch"), QStringLiteral("--unset-upstream"), branch } };
}

void RemoteSync::setUpstream(const QString &branch, const QString &upstream)
{
    if (branch.isEmpty() || upstream.isEmpty())
        return;
    m_repository->runUndoable(tr("%1 now tracks %2").arg(branch, upstream),
                              undo::fixed(tr("set upstream of %1").arg(branch), restoreUpstream(branch)),
                                    [branch, upstream](const QString &worktree, const RunOptions &) {
        return gitsync::setUpstream(worktree, branch, upstream);
    });
}

void RemoteSync::unsetUpstream(const QString &branch)
{
    if (branch.isEmpty())
        return;
    m_repository->runUndoable(tr("%1 no longer tracks an upstream").arg(branch),
                              undo::fixed(tr("unset upstream of %1").arg(branch), restoreUpstream(branch)),
                                    [branch](const QString &worktree, const RunOptions &) {
        return gitsync::unsetUpstream(worktree, branch);
    });
}

} // namespace GitNaga
