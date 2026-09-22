#include "repository_controller.hpp"

#include "git_client.hpp"

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>

#include <QClipboard>
#include <QGuiApplication>

namespace GitNaga {
namespace {

QString shortOid(const QString &oid)
{
    return oid.left(8);
}

const QStringList resetModes = { QStringLiteral("soft"), QStringLiteral("mixed"), QStringLiteral("hard") };

} // namespace

void RepositoryController::copyToClipboard(const QString &text) const
{
    if (auto *clipboard = QGuiApplication::clipboard())
        clipboard->setText(text);
}

void RepositoryController::checkoutCommit(const QString &oid)
{
    if (oid.isEmpty())
        return;
    runOperation(QStringLiteral("switch to commit"),
                 { QStringLiteral("checkout"), QStringLiteral("--detach"), oid },
                 tr("Checked out %1").arg(shortOid(oid)));
}

void RepositoryController::checkoutBranch(const QString &name)
{
    if (name.isEmpty())
        return;
    runOperation(QStringLiteral("switch branch"),
                 { QStringLiteral("checkout"), name },
                 tr("Switched to %1").arg(name));
}

void RepositoryController::createBranch(const QString &name, const QString &oid)
{
    if (name.isEmpty() || oid.isEmpty())
        return;
    runOperation(QStringLiteral("create branch"),
                 { QStringLiteral("branch"), name, oid },
                 tr("Created branch %1 at %2").arg(name, shortOid(oid)));
}

void RepositoryController::commitWorktree(const QString &summary, const QString &description)
{
    if (m_repository.worktree.isEmpty() || m_operationActive)
        return;
    const auto worktree = m_repository.worktree;
    const auto include = m_changedFiles.selectedPaths();
    const auto unstage = m_changedFiles.stagedUnselectedPaths();
    if (include.isEmpty()) {
        setOperationMessage(tr("Select at least one file to commit"));
        emit operationFinished(false, tr("Select at least one file to commit"));
        return;
    }
    m_operationActive = true;
    emit busyChanged();

    auto *watcher = new QFutureWatcher<GitResult<QString>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
        const auto result = watcher->result();
        watcher->deleteLater();
        m_operationActive = false;
        emit busyChanged();
        if (!result) {
            const auto message = result.error().message.isEmpty() ? result.error().operation : result.error().message;
            setOperationMessage(message);
            emit operationFinished(false, message);
            return;
        }
        setOperationMessage(tr("Committed %1").arg(result->left(8)));
        emit operationFinished(true, tr("Committed %1").arg(result->left(8)));
        refresh();
    });
    watcher->setFuture(QtConcurrent::run([worktree, include, unstage, summary, description] {
        return GitClient::commitWorktree(worktree, include, unstage, summary, description);
    }));
}

void RepositoryController::deleteBranch(const QString &name)
{
    if (name.isEmpty())
        return;
    runOperation(QStringLiteral("delete branch"),
                 { QStringLiteral("branch"), QStringLiteral("-D"), name },
                 tr("Deleted branch %1").arg(name));
}

void RepositoryController::createTag(const QString &name, const QString &oid)
{
    if (name.isEmpty() || oid.isEmpty())
        return;
    runOperation(QStringLiteral("create tag"),
                 { QStringLiteral("tag"), name, oid },
                 tr("Tagged %1 as %2").arg(shortOid(oid), name));
}

void RepositoryController::cherryPick(const QString &oid)
{
    if (oid.isEmpty())
        return;
    runOperation(QStringLiteral("cherry-pick"),
                 { QStringLiteral("cherry-pick"), oid },
                 tr("Cherry-picked %1").arg(shortOid(oid)));
}

void RepositoryController::revertCommit(const QString &oid)
{
    if (oid.isEmpty())
        return;
    runOperation(QStringLiteral("revert commit"),
                 { QStringLiteral("revert"), QStringLiteral("--no-edit"), oid },
                 tr("Reverted %1").arg(shortOid(oid)));
}

void RepositoryController::mergeCommit(const QString &oid)
{
    if (oid.isEmpty())
        return;
    runOperation(QStringLiteral("merge commit"),
                 { QStringLiteral("merge"), QStringLiteral("--no-edit"), oid },
                 tr("Merged %1").arg(shortOid(oid)));
}

void RepositoryController::resetTo(const QString &oid, const QString &mode)
{
    if (oid.isEmpty() || !resetModes.contains(mode))
        return;
    runOperation(QStringLiteral("reset branch"),
                 { QStringLiteral("reset"), QStringLiteral("--") + mode, oid },
                 tr("Reset %1 to %2").arg(mode, shortOid(oid)));
}

void RepositoryController::rebaseOnto(const QString &oid)
{
    if (oid.isEmpty())
        return;
    runOperation(QStringLiteral("rebase branch"),
                 { QStringLiteral("rebase"), oid },
                 tr("Rebased onto %1").arg(shortOid(oid)));
}

} // namespace GitNaga
