#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>

namespace GitNaga {

class RepositoryController;

// The operation git left half done (a merge, rebase, cherry-pick, revert or
// bisect) and the conflicts it stopped on. QML reaches it as
// `repository.operation`; what is in progress is `repository.inProgress`.
class OperationController final : public QObject
{
    Q_OBJECT

public:
    explicit OperationController(RepositoryController *repository);

    Q_INVOKABLE void continueOperation();
    Q_INVOKABLE void skipOperation();
    Q_INVOKABLE void abortOperation();
    Q_INVOKABLE void bisect(const QString &verdict);
    Q_INVOKABLE void resolveConflict(const QString &path, const QString &side);
    Q_INVOKABLE void markResolved(const QString &path);
    Q_INVOKABLE void openInEditor(const QString &path) const;
    Q_INVOKABLE void openMergetool(const QString &path) const;
    // The working file split into text and conflict blocks for the conflict view.
    Q_INVOKABLE QVariantMap conflictDocument(const QString &path) const;
    // One choice per conflict ("ours", "theirs", "ours-theirs", "theirs-ours").
    Q_INVOKABLE void saveConflictResolution(const QString &path, const QStringList &choices);

private:
    RepositoryController *m_repository;
};

} // namespace GitNaga
