#include "repository_controller.hpp"

#include <QDir>
#include <QFileInfo>

namespace GitNaga {

namespace {

// Git's own message for a stopped merge lists every CONFLICT line; the banner
// shows those files already, so the footer says what to do next instead.
bool stoppedMidOperation(const QString &gitDirectory)
{
    const QDir git(gitDirectory);
    for (const auto &name : { QStringLiteral("MERGE_HEAD"), QStringLiteral("CHERRY_PICK_HEAD"),
                              QStringLiteral("REVERT_HEAD"), QStringLiteral("rebase-merge"),
                              QStringLiteral("rebase-apply") }) {
        if (QFileInfo::exists(git.filePath(name)))
            return true;
    }
    return false;
}

} // namespace

void RepositoryController::runUndoable(const QString &successMessage, UndoCapture capture, OperationWork work)
{
    auto entry = std::make_shared<UndoEntry>();
    const auto gitDirectory = m_repository.location.gitDirectory;
    const bool alreadyStopped = !m_repository.status.operation.isEmpty();
    runAsyncOperation(
        successMessage,
        [capture = std::move(capture), work = std::move(work), entry, gitDirectory, alreadyStopped](
            const QString &worktree, const RunOptions &options) -> GitResult<QString> {
            if (capture)
                *entry = capture(worktree);
            auto result = work(worktree, options);
            if (!entry->requireGone.isEmpty() && QFileInfo::exists(entry->requireGone))
                entry->steps.clear();
            if (!result && !alreadyStopped && stoppedMidOperation(gitDirectory)) {
                auto error = result.error();
                error.message = tr("Stopped on conflicts: resolve them above the graph, then Continue or Abort");
                return std::unexpected(error);
            }
            return result;
        },
        [this, entry](const QString &worktree, const GitResult<QString> &result) {
            if (!result)
                return;
            if (entry->valid())
                m_undo.push(*entry, worktree);
            else if (!entry->label.isEmpty())
                m_undo.block(entry->label, worktree);
        });
}

void RepositoryController::runUndoable(const QString &operation, const QStringList &arguments,
                                       const QString &successMessage, UndoCapture capture)
{
    runUndoable(successMessage, std::move(capture),
                [arguments, operation](const QString &worktree, const RunOptions &options) {
                    return GitClient::mutate(worktree, arguments, operation, options);
                });
}

void RepositoryController::undo()
{
    const auto *entry = m_undo.top();
    if (!entry) {
        reportFailure(m_undo.label().isEmpty() ? tr("Nothing to undo") : m_undo.label());
        return;
    }
    // A queued undo would be planned against refs that are about to move.
    if (busy()) {
        reportFailure(tr("Wait for the running operation before undoing"));
        return;
    }
    if (!m_repository.status.operation.isEmpty()) {
        reportFailure(tr("Finish or abort the %1 before undoing").arg(m_repository.status.operation));
        return;
    }
    const auto id = m_undo.topId();
    const auto label = entry->label;
    const auto steps = entry->steps;
    runAsyncOperation(
        tr("Undid %1").arg(label),
        [steps, label](const QString &worktree, const RunOptions &options) -> GitResult<QString> {
            QString output;
            for (const auto &step : steps) {
                auto result = GitClient::mutate(worktree, step, QStringLiteral("undo ") + label, options);
                if (!result) {
                    auto error = result.error();
                    // `reset --keep` refuses rather than overwrite edits; say
                    // that is why, then git's own words.
                    error.message = tr("Cannot undo %1: %2").arg(label, error.message);
                    return std::unexpected(error);
                }
                output = *result;
            }
            return output;
        },
        [this, id](const QString &, const GitResult<QString> &result) {
            if (result)
                m_undo.pop(id);
        });
}

void RepositoryController::dropStash(const QString &selector, const QString &message)
{
    if (selector.isEmpty())
        return;
    runUndoable(QStringLiteral("drop stash"), { QStringLiteral("stash"), QStringLiteral("drop"), selector },
                tr("Dropped %1").arg(selector),
                undo::restoreStash(tr("drop %1").arg(selector), selector, message));
}

} // namespace GitNaga
