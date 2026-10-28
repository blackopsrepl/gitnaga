#include "repository_controller.hpp"

#include "git_client.hpp"
#include "git_refs.hpp"

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
    runUndoable(QStringLiteral("switch to commit"),
                { QStringLiteral("checkout"), QStringLiteral("--detach"), oid },
                tr("Checked out %1").arg(shortOid(oid)), undo::returnTo(tr("checkout")));
}

void RepositoryController::checkoutBranch(const QString &name)
{
    if (name.isEmpty())
        return;
    runUndoable(QStringLiteral("switch branch"),
                { QStringLiteral("checkout"), name },
                tr("Switched to %1").arg(name), undo::returnTo(tr("checkout")));
}

void RepositoryController::checkoutRemoteBranch(const QString &reference)
{
    QString remote;
    QString branch;
    if (!gitrefs::splitRemoteRef(reference, &remote, &branch))
        return;
    for (const auto &ref : m_repository.refs.refs) {
        if (ref.kind == RefKind::Local && ref.name == branch) {
            checkoutBranch(branch);
            return;
        }
    }
    runUndoable(QStringLiteral("track remote branch"),
                { QStringLiteral("switch"), QStringLiteral("--track"), reference },
                tr("Switched to new branch %1 tracking %2").arg(branch, reference),
                undo::returnTo(tr("checkout"), { { QStringLiteral("branch"), QStringLiteral("-D"), branch } }));
}

void RepositoryController::createBranch(const QString &name, const QString &oid)
{
    if (name.isEmpty() || oid.isEmpty())
        return;
    runUndoable(QStringLiteral("create branch"),
                { QStringLiteral("branch"), name, oid },
                tr("Created branch %1 at %2").arg(name, shortOid(oid)),
                undo::fixed(tr("create branch %1").arg(name), { { QStringLiteral("branch"), QStringLiteral("-D"), name } }));
}

void RepositoryController::commitWorktree(const QString &summary, const QString &description)
{
    const auto include = m_changedFiles.selectedPaths();
    const auto unstage = m_changedFiles.stagedUnselectedPaths();
    if (include.isEmpty()) {
        reportFailure(tr("Select at least one file to commit"));
        return;
    }
    // Undone with a soft reset, so the commit's changes come back as staged work.
    runUndoable(tr("Committed %1").arg(summary.left(48)), undo::resetHead(tr("commit"), QStringLiteral("--soft")),
                      [include, unstage, summary, description](const QString &worktree, const RunOptions &) {
                          return GitClient::commitWorktree(worktree, include, unstage, summary, description);
                      });
}

void RepositoryController::deleteBranch(const QString &name)
{
    if (name.isEmpty())
        return;
    // Deleting a branch drops its upstream setting too; the undo puts it back.
    QVector<QStringList> after;
    for (const auto &ref : m_repository.refs.refs) {
        if (ref.kind == RefKind::Local && ref.name == name && !ref.upstream.isEmpty())
            after.append({ QStringLiteral("branch"), QStringLiteral("--set-upstream-to=") + ref.upstream, name });
    }
    runUndoable(QStringLiteral("delete branch"),
                { QStringLiteral("branch"), QStringLiteral("-D"), name },
                tr("Deleted branch %1").arg(name),
                undo::recreateRef(tr("delete branch %1").arg(name), QStringLiteral("refs/heads/") + name, after));
}

void RepositoryController::deleteRemoteBranch(const QString &reference)
{
    QString remote;
    QString branch;
    if (!gitrefs::splitRemoteRef(reference, &remote, &branch))
        return;
    // The push runs against the remote's own transport and prunes the local
    // remote-tracking ref itself, so there is nothing to reconcile afterwards.
    runUndoable(tr("Deleted %1 on %2").arg(branch, remote), undo::never(tr("delete remote branch")),
                      [remote, branch](const QString &repository, const RunOptions &options) {
                          return GitClient::deleteRemoteBranch(repository, remote, branch, options);
                      });
}

void RepositoryController::createTag(const QString &name, const QString &oid)
{
    if (name.isEmpty() || oid.isEmpty())
        return;
    runUndoable(QStringLiteral("create tag"),
                { QStringLiteral("tag"), name, oid },
                tr("Tagged %1 as %2").arg(shortOid(oid), name),
                undo::fixed(tr("create tag %1").arg(name), { { QStringLiteral("tag"), QStringLiteral("-d"), name } }));
}

void RepositoryController::deleteTag(const QString &name)
{
    if (name.isEmpty())
        return;
    runUndoable(QStringLiteral("delete tag"), { QStringLiteral("tag"), QStringLiteral("-d"), name },
                tr("Deleted tag %1").arg(name),
                undo::recreateRef(tr("delete tag %1").arg(name), QStringLiteral("refs/tags/") + name));
}

void RepositoryController::cherryPick(const QString &oid)
{
    if (oid.isEmpty())
        return;
    runUndoable(QStringLiteral("cherry-pick"),
                { QStringLiteral("cherry-pick"), oid },
                tr("Cherry-picked %1").arg(shortOid(oid)), undo::resetHead(tr("cherry-pick"), QStringLiteral("--keep")));
}

void RepositoryController::revertCommit(const QString &oid)
{
    if (oid.isEmpty())
        return;
    runUndoable(QStringLiteral("revert commit"),
                { QStringLiteral("revert"), QStringLiteral("--no-edit"), oid },
                tr("Reverted %1").arg(shortOid(oid)), undo::resetHead(tr("revert"), QStringLiteral("--keep")));
}

void RepositoryController::mergeCommit(const QString &oid, const QString &branch)
{
    if (oid.isEmpty())
        return;
    const auto target = branch.isEmpty() ? oid : branch;
    runUndoable(QStringLiteral("merge"),
                { QStringLiteral("merge"), QStringLiteral("--no-edit"), target },
                tr("Merged %1").arg(branch.isEmpty() ? shortOid(oid) : branch),
                undo::resetHead(tr("merge"), QStringLiteral("--keep")));
}

void RepositoryController::resetTo(const QString &oid, const QString &mode)
{
    if (oid.isEmpty() || !resetModes.contains(mode))
        return;
    // Soft and mixed resets never touch the worktree, so neither does their
    // undo; a hard reset's discarded edits are gone, only the branch returns.
    const auto inverse = mode == QStringLiteral("hard") ? QStringLiteral("--keep") : QStringLiteral("--") + mode;
    runUndoable(QStringLiteral("reset branch"),
                { QStringLiteral("reset"), QStringLiteral("--") + mode, oid },
                tr("Reset %1 to %2").arg(mode, shortOid(oid)), undo::resetHead(tr("%1 reset").arg(mode), inverse));
}

void RepositoryController::rebaseOnto(const QString &oid, const QString &branch)
{
    if (oid.isEmpty())
        return;
    const auto target = branch.isEmpty() ? oid : branch;
    runUndoable(QStringLiteral("rebase"),
                { QStringLiteral("rebase"), target },
                tr("Rebased onto %1").arg(branch.isEmpty() ? shortOid(oid) : branch),
                undo::resetHead(tr("rebase"), QStringLiteral("--keep")));
}

} // namespace GitNaga
