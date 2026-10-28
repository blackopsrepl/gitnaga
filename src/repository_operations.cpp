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
    runOperation(QStringLiteral("track remote branch"),
                 { QStringLiteral("switch"), QStringLiteral("--track"), reference },
                 tr("Switched to new branch %1 tracking %2").arg(branch, reference));
}

void RepositoryController::createBranch(const QString &name, const QString &oid)
{
    if (name.isEmpty() || oid.isEmpty())
        return;
    runOperation(QStringLiteral("create branch"),
                 { QStringLiteral("branch"), name, oid },
                 tr("Created branch %1 at %2").arg(name, shortOid(oid)));
}

void RepositoryController::deleteBranch(const QString &name)
{
    if (name.isEmpty())
        return;
    runOperation(QStringLiteral("delete branch"),
                 { QStringLiteral("branch"), QStringLiteral("-D"), name },
                 tr("Deleted branch %1").arg(name));
}

void RepositoryController::deleteRemoteBranch(const QString &reference)
{
    QString remote;
    QString branch;
    if (!gitrefs::splitRemoteRef(reference, &remote, &branch))
        return;
    // The push runs against the remote's own transport and prunes the local
    // remote-tracking ref itself, so there is nothing to reconcile afterwards.
    runAsyncOperation(tr("Deleted %1 on %2").arg(branch, remote),
                      [remote, branch](const QString &repository, const RunOptions &options) {
                          return GitClient::deleteRemoteBranch(repository, remote, branch, options);
                      });
}

void RepositoryController::createTag(const QString &name, const QString &oid)
{
    if (name.isEmpty() || oid.isEmpty())
        return;
    runOperation(QStringLiteral("create tag"),
                 { QStringLiteral("tag"), name, oid },
                 tr("Tagged %1 as %2").arg(shortOid(oid), name));
}

void RepositoryController::deleteTag(const QString &name)
{
    if (name.isEmpty())
        return;
    runOperation(QStringLiteral("delete tag"), { QStringLiteral("tag"), QStringLiteral("-d"), name },
                 tr("Deleted tag %1").arg(name));
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

void RepositoryController::mergeCommit(const QString &oid, const QString &branch)
{
    if (oid.isEmpty())
        return;
    const auto target = branch.isEmpty() ? oid : branch;
    runOperation(QStringLiteral("merge"),
                 { QStringLiteral("merge"), QStringLiteral("--no-edit"), target },
                 tr("Merged %1").arg(branch.isEmpty() ? shortOid(oid) : branch));
}

void RepositoryController::resetTo(const QString &oid, const QString &mode)
{
    if (oid.isEmpty() || !resetModes.contains(mode))
        return;
    runOperation(QStringLiteral("reset branch"),
                 { QStringLiteral("reset"), QStringLiteral("--") + mode, oid },
                 tr("Reset %1 to %2").arg(mode, shortOid(oid)));
}

void RepositoryController::rebaseOnto(const QString &oid, const QString &branch)
{
    if (oid.isEmpty())
        return;
    const auto target = branch.isEmpty() ? oid : branch;
    runOperation(QStringLiteral("rebase"),
                 { QStringLiteral("rebase"), target },
                 tr("Rebased onto %1").arg(branch.isEmpty() ? shortOid(oid) : branch));
}

} // namespace GitNaga
