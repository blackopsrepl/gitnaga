#pragma once

#include "commit_model.hpp"
#include "diff_line_model.hpp"
#include "file_change_model.hpp"
#include "git_client.hpp"

#include <QFileSystemWatcher>
#include <QObject>
#include <QQueue>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include <functional>

namespace GitNaga {

class RepositoryController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(CommitModel *commits READ commits CONSTANT)
    Q_PROPERTY(FileChangeModel *changedFiles READ changedFiles CONSTANT)
    Q_PROPERTY(DiffLineModel *diffLines READ diffLines CONSTANT)
    Q_PROPERTY(FileChangeModel *unstagedFiles READ unstagedFiles CONSTANT)
    Q_PROPERTY(FileChangeModel *stagedFiles READ stagedFiles CONSTANT)
    // "unstaged" or "staged" while a work-in-progress file is shown.
    Q_PROPERTY(QString wipSide READ wipSide NOTIFY fileSelectionChanged)
    Q_PROPERTY(int stagedCount READ stagedCount NOTIFY statusChanged)
    Q_PROPERTY(QString repositoryPath READ repositoryPath NOTIFY repositoryChanged)
    Q_PROPERTY(QString repositoryName READ repositoryName NOTIFY repositoryChanged)
    Q_PROPERTY(QString currentBranch READ currentBranch NOTIFY referencesChanged)
    Q_PROPERTY(QString headOid READ headOid NOTIFY referencesChanged)
    Q_PROPERTY(bool detached READ detached NOTIFY referencesChanged)
    Q_PROPERTY(int commitCount READ commitCount NOTIFY historyChanged)
    Q_PROPERTY(bool historyLoading READ historyLoading NOTIFY historyChanged)
    Q_PROPERTY(QString workInProgressOid READ workInProgressOid NOTIFY statusChanged)
    Q_PROPERTY(int uncommittedCount READ uncommittedCount NOTIFY statusChanged)
    Q_PROPERTY(QString operationInProgress READ operationInProgress NOTIFY statusChanged)
    Q_PROPERTY(QString upstream READ upstream NOTIFY statusChanged)
    Q_PROPERTY(int ahead READ ahead NOTIFY statusChanged)
    Q_PROPERTY(int behind READ behind NOTIFY statusChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(int pendingOperations READ pendingOperations NOTIFY busyChanged)
    Q_PROPERTY(QString operationProgress READ operationProgress NOTIFY operationProgressChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)
    Q_PROPERTY(QString operationMessage READ operationMessage NOTIFY operationMessageChanged)
    Q_PROPERTY(bool lastOperationOk READ lastOperationOk NOTIFY operationMessageChanged)
    Q_PROPERTY(int selectedRow READ selectedRow NOTIFY selectionChanged)
    Q_PROPERTY(int selectedFile READ selectedFile NOTIFY fileSelectionChanged)
    Q_PROPERTY(QVariantList references READ references NOTIFY referencesChanged)
    Q_PROPERTY(QVariantList stashes READ stashes NOTIFY referencesChanged)
    Q_PROPERTY(bool showInternalRefs READ showInternalRefs WRITE setShowInternalRefs NOTIFY referencesChanged)
    Q_PROPERTY(QStringList hiddenRefs READ hiddenRefs NOTIFY referencesChanged)
    Q_PROPERTY(QString soloRef READ soloRef NOTIFY referencesChanged)
    Q_PROPERTY(QVariantList worktrees READ worktrees NOTIFY repositoryChanged)
    Q_PROPERTY(QString selectedOid READ selectedOid NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedSubject READ selectedSubject NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedAuthor READ selectedAuthor NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedDate READ selectedDate NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedBody READ selectedBody NOTIFY selectionChanged)
    Q_PROPERTY(QStringList selectedParents READ selectedParents NOTIFY selectionChanged)

public:
    explicit RepositoryController(QObject *parent = nullptr);
    ~RepositoryController() override;

    CommitModel *commits();
    FileChangeModel *changedFiles();
    DiffLineModel *diffLines();
    FileChangeModel *unstagedFiles() { return &m_unstagedFiles; }
    FileChangeModel *stagedFiles() { return &m_stagedFiles; }
    QString wipSide() const;
    int stagedCount() const;
    QString repositoryPath() const;
    QString repositoryName() const;
    QString currentBranch() const;
    QString headOid() const;
    bool detached() const;
    int commitCount() const;
    bool historyLoading() const;
    QString workInProgressOid() const;
    int uncommittedCount() const;
    QString operationInProgress() const;
    QString upstream() const;
    int ahead() const;
    int behind() const;
    bool loading() const;
    bool busy() const;
    int pendingOperations() const;
    QString operationProgress() const;
    QString errorMessage() const;
    QString operationMessage() const;
    bool lastOperationOk() const;
    int selectedRow() const;
    int selectedFile() const;
    QVariantList references() const;
    QVariantList stashes() const;
    bool showInternalRefs() const;
    void setShowInternalRefs(bool show);
    QStringList hiddenRefs() const;
    QString soloRef() const;
    QVariantList worktrees() const;
    QString selectedOid() const;
    QString selectedSubject() const;
    QString selectedAuthor() const;
    QString selectedDate() const;
    QString selectedBody() const;
    QStringList selectedParents() const;
    const RepositorySnapshot &repository() const { return m_repository; }

    Q_INVOKABLE void openRepository(const QUrl &url);
    Q_INVOKABLE void openRepositoryPath(const QString &path);
    // Everything again: refs, status and the whole history.
    Q_INVOKABLE void refresh();
    // Refs and status; the history only when a ref moved.
    Q_INVOKABLE void synchronize();
    Q_INVOKABLE void selectCommit(int row);
    Q_INVOKABLE void selectOid(const QString &oid);
    Q_INVOKABLE void selectFile(int row);
    Q_INVOKABLE void cancelOperation();
    Q_INVOKABLE void setRefHidden(const QString &fullName, bool hidden);
    Q_INVOKABLE void setSoloRef(const QString &fullName);
    Q_INVOKABLE void copyToClipboard(const QString &text) const;
    // Commits the index; amending rewrites HEAD with it.
    Q_INVOKABLE void commitWorktree(const QString &summary, const QString &description, bool amend = false);
    // Staging (repository_staging.cpp). Rows index the side's own list;
    // `action` is "stage", "unstage" or "discard".
    Q_INVOKABLE void selectWorktreeFile(const QString &side, int row);
    Q_INVOKABLE void stageFile(int row);
    Q_INVOKABLE void unstageFile(int row);
    Q_INVOKABLE void discardFile(int row);
    Q_INVOKABLE void stageAll();
    Q_INVOKABLE void unstageAll();
    Q_INVOKABLE void discardAll();
    Q_INVOKABLE void applyHunk(int hunkRow, const QString &action);
    Q_INVOKABLE void applyLines(const QVariantList &rows, const QString &action);
    Q_INVOKABLE void loadAmendMessage();
    Q_INVOKABLE void stashWorktree(const QString &message, bool includeUntracked);
    Q_INVOKABLE void stashAction(const QString &action, const QString &selector, const QString &oid);
    Q_INVOKABLE QVariantList directories(const QString &path) const;
    Q_INVOKABLE QString homeDirectory() const;
    Q_INVOKABLE bool looksLikeRepository(const QString &path) const;
    Q_INVOKABLE QStringList recentProjects() const;
    Q_INVOKABLE QVariantList fuzzyMatchProjects(const QString &needle) const;
    Q_INVOKABLE void openWorktree(const QString &path);
    Q_INVOKABLE void addWorktree(const QString &path, const QString &branch, const QString &mode,
                                 const QString &startPoint);
    Q_INVOKABLE void removeWorktree(const QString &path, bool force);
    Q_INVOKABLE void lockWorktree(const QString &path, const QString &reason);
    Q_INVOKABLE void unlockWorktree(const QString &path);
    Q_INVOKABLE void moveWorktree(const QString &path, const QString &newPath);
    Q_INVOKABLE void pruneWorktrees();
    Q_INVOKABLE void repairWorktrees();

    Q_INVOKABLE void checkoutCommit(const QString &oid);
    Q_INVOKABLE void checkoutBranch(const QString &name);
    // A remote branch checks out as the local branch that tracks it, created
    // on first use, the way a person means "work on origin/x".
    Q_INVOKABLE void checkoutRemoteBranch(const QString &reference);
    Q_INVOKABLE void createBranch(const QString &name, const QString &oid);
    Q_INVOKABLE void deleteBranch(const QString &name);
    // Delete a branch on one remote, addressed by its remote-tracking ref short
    // name ("<remote>/<branch>"). The ref is named, not the remote URL, because
    // GitHub redirects a renamed repository and the stale URL would fail.
    Q_INVOKABLE void deleteRemoteBranch(const QString &reference);
    Q_INVOKABLE void createTag(const QString &name, const QString &oid);
    Q_INVOKABLE void deleteTag(const QString &name);
    Q_INVOKABLE void cherryPick(const QString &oid);
    Q_INVOKABLE void revertCommit(const QString &oid);
    // Merges a branch by name when one is given, so the merge commit says
    // "Merge branch 'x'" rather than naming a bare hash.
    Q_INVOKABLE void mergeCommit(const QString &oid, const QString &branch = {});
    Q_INVOKABLE void resetTo(const QString &oid, const QString &mode);
    Q_INVOKABLE void rebaseOnto(const QString &oid, const QString &branch = {});

    // Off-thread git work: the worktree it runs in and the run options that
    // carry the cancel token and the progress reporter.
    using OperationWork = std::function<GitResult<QString>(const QString &worktree, const RunOptions &options)>;
    using OperationResult = std::function<void(const QString &worktree, const GitResult<QString> &result)>;
    // Queue git work behind whatever is running. Nothing is dropped: a second
    // click while busy runs once the first operation lands.
    void runAsyncOperation(const QString &successMessage, OperationWork work, OperationResult result = {});
    void runOperation(const QString &operation, const QStringList &arguments, const QString &successMessage);
    void reportFailure(const QString &message);

signals:
    void repositoryChanged();
    void historyChanged();
    void statusChanged();
    void loadingChanged();
    void busyChanged();
    void errorChanged();
    void operationMessageChanged();
    void operationProgressChanged();
    void operationFinished(bool ok, const QString &message);
    void selectionChanged();
    // The user asked to look at a row (a click, a ref, a search hit), as
    // opposed to a refresh re-finding the selection: only this scrolls.
    void revealRow(int row);
    void fileSelectionChanged();
    void referencesChanged();
    void amendMessageLoaded(const QString &summary, const QString &description, bool published);

private:
    struct PendingOperation {
        QString successMessage;
        OperationWork work;
        OperationResult result;
    };
    struct SyncResult;

    void startSync(bool forceHistory);
    void applySync(const SyncResult &result, bool forceHistory);
    void loadHistory();
    void pollStatus();
    void applyStatus(WorktreeStatus status);
    void startNextOperation();
    void setLoading(bool loading);
    void setError(QString message);
    void setOperationMessage(QString message, bool ok = true);
    void setOperationProgress(QString progress);
    void configureWatcher();
    void clearSelection();
    void restoreSelection();
    void inspect(const QString &oid, int row);
    void rebuildReferences();
    void loadViewSettings();
    void saveViewSettings() const;
    void showWorktree();
    void runStaging(const QString &successMessage, OperationWork work);
    void applyPatchRows(const QSet<int> &rows, const QString &action);
    bool isStash(const QString &oid) const;

    CommitModel m_commits;
    FileChangeModel m_changedFiles;
    DiffLineModel m_diffLines;
    FileChangeModel m_unstagedFiles;
    FileChangeModel m_stagedFiles;
    QFileSystemWatcher m_watcher;
    QTimer m_syncTimer;
    QTimer m_statusTimer;
    QTimer m_messageTimer;
    RepositorySnapshot m_repository;
    CommitDetails m_selected;
    QString m_selectedOid;
    QString m_selectedPath;
    DiffSide m_selectedSide = DiffSide::Commit;
    // The (side, path) the loaded diff belongs to; line actions refuse a
    // diff that is still catching up with the selection.
    QString m_diffKey;
    QString m_requestedPath;
    QString m_recordedWorktree;
    QString m_error;
    QString m_operationMessage;
    QString m_operationProgress;
    QVariantList m_references;
    HistoryRequest m_view;
    QQueue<PendingOperation> m_operations;
    CancelToken m_operationCancel;
    CancelToken m_historyCancel;
    int m_selectedRow = -1;
    int m_selectedFile = -1;
    bool m_loading = false;
    bool m_historyLoading = false;
    bool m_operationActive = false;
    bool m_statusPolling = false;
    bool m_lastOperationOk = true;
    quint64 m_syncGeneration = 0;
    quint64 m_historyGeneration = 0;
    quint64 m_selectionGeneration = 0;
    quint64 m_diffGeneration = 0;
};

} // namespace GitNaga
