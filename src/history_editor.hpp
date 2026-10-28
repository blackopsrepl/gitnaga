#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace GitNaga {

class RepositoryController;

// Rewriting history and moving branches by hand: the interactive rebase
// editor, the commit menu's quick rewrites, and branch drag and drop. QML
// reaches it as `repository.history`. Every rewrite of the checked-out branch
// is undoable.
class HistoryEditor final : public QObject
{
    Q_OBJECT

public:
    explicit HistoryEditor(RepositoryController *repository);

    // Answers through rebasePlanReady: the commits from `oid` to HEAD.
    Q_INVOKABLE void requestRebasePlan(const QString &oid);
    // `base` empty means --root; steps are {oid, action, original, message}.
    Q_INVOKABLE void startInteractiveRebase(const QString &base, const QVariantList &steps, bool autostash);
    Q_INVOKABLE void rewordCommit(const QString &oid, const QString &message);
    Q_INVOKABLE void squashIntoParent(const QString &oid);
    Q_INVOKABLE void dropCommit(const QString &oid);
    Q_INVOKABLE bool reachableFromHead(const QString &oid) const;
    Q_INVOKABLE QString combineMessages(const QStringList &messages) const;
    Q_INVOKABLE void moveBranch(const QString &name, const QString &oid);
    Q_INVOKABLE void rebaseBranch(const QString &branch, const QString &onto);
    Q_INVOKABLE void mergeBranchInto(const QString &source, const QString &target);

signals:
    // {ok, error, oid, base, steps: [{oid, shortOid, subject, original}]}
    void rebasePlanReady(const QVariantMap &plan);

private:
    RepositoryController *m_repository;
};

} // namespace GitNaga
