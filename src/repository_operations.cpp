#include "repository_controller.hpp"

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
