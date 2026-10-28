#include "repository_controller.hpp"

#include "git_rebase.hpp"

#include <QFutureWatcher>
#include <QSet>
#include <QVariantMap>
#include <QtConcurrentRun>

namespace GitNaga {

namespace {

QString shortOid(const QString &oid)
{
    return oid.left(8);
}

QVariantMap planToVariant(const QString &oid, const GitResult<gitrebase::Range> &range)
{
    QVariantMap plan;
    plan.insert(QStringLiteral("oid"), oid);
    plan.insert(QStringLiteral("ok"), range.has_value());
    if (!range) {
        plan.insert(QStringLiteral("error"), range.error().message);
        return plan;
    }
    plan.insert(QStringLiteral("base"), range->base);
    QVariantList steps;
    for (const auto &step : range->steps) {
        steps.append(QVariantMap{
            { QStringLiteral("oid"), step.oid },
            { QStringLiteral("shortOid"), shortOid(step.oid) },
            { QStringLiteral("subject"), step.subject },
            { QStringLiteral("original"), step.original },
        });
    }
    plan.insert(QStringLiteral("steps"), steps);
    return plan;
}

} // namespace

void RepositoryController::requestRebasePlan(const QString &oid)
{
    const auto worktree = m_repository.location.worktree;
    if (worktree.isEmpty() || oid.isEmpty())
        return;
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
        watcher->deleteLater();
        emit rebasePlanReady(watcher->result());
    });
    watcher->setFuture(QtConcurrent::run([worktree, oid] { return planToVariant(oid, gitrebase::loadRange(worktree, oid)); }));
}

void RepositoryController::startInteractiveRebase(const QString &base, const QVariantList &steps, bool autostash)
{
    QVector<rebase::Step> plan;
    for (const auto &value : steps) {
        const auto map = value.toMap();
        const auto action = rebase::parseAction(map.value(QStringLiteral("action")).toString());
        if (!action) {
            reportFailure(tr("Unknown rebase action %1").arg(map.value(QStringLiteral("action")).toString()));
            return;
        }
        plan.append({ *action, map.value(QStringLiteral("oid")).toString(), map.value(QStringLiteral("subject")).toString(),
                      map.value(QStringLiteral("original")).toString(), map.value(QStringLiteral("message")).toString() });
    }
    const auto count = plan.size();
    runAsyncOperation(count == 1 ? tr("Rebased 1 commit") : tr("Rebased %1 commits").arg(count),
                      [base, plan, autostash](const QString &worktree, const RunOptions &options) {
                          return gitrebase::run(worktree, base, plan, autostash, options);
                      });
}

void RepositoryController::rewordCommit(const QString &oid, const QString &message)
{
    if (oid.isEmpty() || message.trimmed().isEmpty())
        return;
    runAsyncOperation(tr("Reworded %1").arg(shortOid(oid)), [oid, message](const QString &worktree, const RunOptions &options) {
        return gitrebase::reword(worktree, oid, message, options);
    });
}

void RepositoryController::squashIntoParent(const QString &oid)
{
    if (oid.isEmpty())
        return;
    runAsyncOperation(tr("Squashed %1 into its parent").arg(shortOid(oid)),
                      [oid](const QString &worktree, const RunOptions &options) {
                          return gitrebase::squashIntoParent(worktree, oid, options);
                      });
}

void RepositoryController::dropCommit(const QString &oid)
{
    if (oid.isEmpty())
        return;
    runAsyncOperation(tr("Dropped %1").arg(shortOid(oid)), [oid](const QString &worktree, const RunOptions &options) {
        return gitrebase::drop(worktree, oid, options);
    });
}

bool RepositoryController::reachableFromHead(const QString &oid) const
{
    // Walks the loaded graph rather than asking git, so a menu can decide
    // what to offer without blocking; HEAD's history is always loaded.
    const auto &head = m_repository.refs.headOid;
    if (oid.isEmpty() || head.isEmpty() || oid == worktreeOid())
        return false;
    QSet<QString> seen{ head };
    QVector<QString> pending{ head };
    while (!pending.isEmpty()) {
        const auto current = pending.takeLast();
        if (current == oid)
            return true;
        const auto *commit = m_commits.commitAt(m_commits.rowOf(current));
        if (!commit)
            continue;
        for (const auto &parent : commit->parents) {
            if (!seen.contains(parent)) {
                seen.insert(parent);
                pending.append(parent);
            }
        }
    }
    return false;
}

QString RepositoryController::combineMessages(const QStringList &messages) const
{
    return rebase::combineMessages(messages);
}

void RepositoryController::moveBranch(const QString &name, const QString &oid)
{
    if (name.isEmpty() || oid.isEmpty())
        return;
    if (name == currentBranch()) {
        reportFailure(tr("%1 is checked out; reset it instead of moving it").arg(name));
        return;
    }
    runOperation(QStringLiteral("move branch"), { QStringLiteral("branch"), QStringLiteral("--force"), name, oid },
                 tr("Moved %1 to %2").arg(name, shortOid(oid)));
}

void RepositoryController::rebaseBranch(const QString &branch, const QString &onto)
{
    if (branch.isEmpty() || onto.isEmpty())
        return;
    // `git rebase <onto> <branch>` checks the branch out first, which is
    // what rebasing a branch that is not checked out has to mean.
    runOperation(QStringLiteral("rebase"), { QStringLiteral("rebase"), onto, branch },
                 tr("Rebased %1 onto %2").arg(branch, onto.size() == 40 ? shortOid(onto) : onto));
}

void RepositoryController::mergeBranchInto(const QString &source, const QString &target)
{
    if (source.isEmpty() || target.isEmpty() || source == target)
        return;
    if (target == currentBranch()) {
        mergeCommit(source, source);
        return;
    }
    // Merging into another branch needs that branch checked out; the switch
    // is part of the operation, as it is when a person types it.
    runAsyncOperation(tr("Merged %1 into %2").arg(source, target),
                      [source, target](const QString &worktree, const RunOptions &options) -> GitResult<QString> {
                          auto switched = GitClient::mutate(worktree, { QStringLiteral("switch"), target },
                                                            QStringLiteral("switch branch"), options);
                          if (!switched)
                              return switched;
                          return GitClient::mutate(worktree, { QStringLiteral("merge"), QStringLiteral("--no-edit"), source },
                                                   QStringLiteral("merge"), options);
                      });
}

} // namespace GitNaga
