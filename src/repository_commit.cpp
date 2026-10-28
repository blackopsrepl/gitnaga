#include "repository_controller.hpp"

#include "git_staging.hpp"
#include "undo_stack.hpp"

#include <QFutureWatcher>
#include <QtConcurrentRun>

namespace GitNaga {

void RepositoryController::commitWorktree(const QString &summary, const QString &description, bool amend)
{
    // Amending with nothing staged is how a message gets reworded.
    if (!amend && stagedCount() == 0) {
        reportFailure(tr("Stage changes before committing"));
        return;
    }
    const auto message = amend ? tr("Amended %1") : tr("Committed %1");
    // Undone with a soft reset: the commit's changes come back staged, and an
    // amend gives back the commit it replaced.
    runUndoable(message.arg(summary.left(48)),
                undo::resetHead(amend ? tr("amend") : tr("commit"), QStringLiteral("--soft")),
                      [summary, description, amend](const QString &worktree, const RunOptions &options) {
                          return staging::commit(worktree, summary, description, amend, options);
                      });
}

void RepositoryController::loadAmendMessage()
{
    const auto worktree = m_repository.location.worktree;
    if (worktree.isEmpty() || m_repository.refs.headOid.isEmpty())
        return;
    auto *watcher = new QFutureWatcher<GitResult<staging::HeadMessage>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
        const auto result = watcher->result();
        watcher->deleteLater();
        if (!result) {
            reportFailure(result.error().message);
            return;
        }
        emit amendMessageLoaded(result->summary, result->description, result->published);
    });
    watcher->setFuture(QtConcurrent::run([worktree] { return staging::headMessage(worktree); }));
}

void RepositoryController::stashWorktree(const QString &message, bool includeUntracked)
{
    runAsyncOperation(tr("Stashed changes"),
                      [message, includeUntracked](const QString &worktree, const RunOptions &options) {
                          return staging::stashPush(worktree, message, includeUntracked, options);
                      });
}

void RepositoryController::stashAction(const QString &action, const QString &selector, const QString &oid)
{
    const auto message = action == QStringLiteral("apply") ? tr("Applied %1")
                       : tr("Popped %1");
    runAsyncOperation(message.arg(selector),
                      [action, selector, oid](const QString &worktree, const RunOptions &options) {
                          return staging::stashAction(worktree, action, selector, oid, options);
                      });
}

} // namespace GitNaga
