#pragma once

#include "commit_model.hpp"
#include "diff_line_model.hpp"
#include "file_change_model.hpp"

#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

namespace GitNaga {

class RepositoryController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(CommitModel *commits READ commits CONSTANT)
    Q_PROPERTY(FileChangeModel *changedFiles READ changedFiles CONSTANT)
    Q_PROPERTY(DiffLineModel *diffLines READ diffLines CONSTANT)
    Q_PROPERTY(QString repositoryPath READ repositoryPath NOTIFY repositoryChanged)
    Q_PROPERTY(QString repositoryName READ repositoryName NOTIFY repositoryChanged)
    Q_PROPERTY(QString currentBranch READ currentBranch NOTIFY repositoryChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)
    Q_PROPERTY(QString operationMessage READ operationMessage NOTIFY operationMessageChanged)
    Q_PROPERTY(int selectedRow READ selectedRow NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList references READ references NOTIFY referencesChanged)
    Q_PROPERTY(QString selectedOid READ selectedOid NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedSubject READ selectedSubject NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedAuthor READ selectedAuthor NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedDate READ selectedDate NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedBody READ selectedBody NOTIFY selectionChanged)

public:
    explicit RepositoryController(QObject *parent = nullptr);

    CommitModel *commits();
    FileChangeModel *changedFiles();
    DiffLineModel *diffLines();
    QString repositoryPath() const;
    QString repositoryName() const;
    QString currentBranch() const;
    bool loading() const;
    bool busy() const;
    QString errorMessage() const;
    QString operationMessage() const;
    int selectedRow() const;
    QVariantList references() const;
    QString selectedOid() const;
    QString selectedSubject() const;
    QString selectedAuthor() const;
    QString selectedDate() const;
    QString selectedBody() const;

    Q_INVOKABLE void openRepository(const QUrl &url);
    Q_INVOKABLE void openRepositoryPath(const QString &path);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void selectCommit(int row);
    Q_INVOKABLE void selectOid(const QString &oid);
    Q_INVOKABLE void selectFile(int row);
    Q_INVOKABLE void copyToClipboard(const QString &text) const;
    Q_INVOKABLE QVariantList directories(const QString &path) const;
    Q_INVOKABLE QString homeDirectory() const;
    Q_INVOKABLE bool looksLikeRepository(const QString &path) const;

    Q_INVOKABLE void checkoutCommit(const QString &oid);
    Q_INVOKABLE void checkoutBranch(const QString &name);
    Q_INVOKABLE void createBranch(const QString &name, const QString &oid);
    Q_INVOKABLE void deleteBranch(const QString &name);
    Q_INVOKABLE void createTag(const QString &name, const QString &oid);
    Q_INVOKABLE void cherryPick(const QString &oid);
    Q_INVOKABLE void revertCommit(const QString &oid);
    Q_INVOKABLE void mergeCommit(const QString &oid);
    Q_INVOKABLE void resetTo(const QString &oid, const QString &mode);
    Q_INVOKABLE void rebaseOnto(const QString &oid);

signals:
    void repositoryChanged();
    void loadingChanged();
    void busyChanged();
    void errorChanged();
    void operationMessageChanged();
    void operationFinished(bool ok, const QString &message);
    void selectionChanged();
    void referencesChanged();

private:
    void runOperation(const QString &operation, const QStringList &arguments, const QString &successMessage);
    void setLoading(bool loading);
    void setError(QString message);
    void setOperationMessage(QString message);
    void configureWatcher();
    void clearSelection();
    void rebuildReferences();

    CommitModel m_commits;
    FileChangeModel m_changedFiles;
    DiffLineModel m_diffLines;
    QFileSystemWatcher m_watcher;
    QTimer m_refreshTimer;
    QTimer m_messageTimer;
    RepositorySnapshot m_repository;
    CommitDetails m_selected;
    QString m_requestedPath;
    QString m_error;
    QString m_operationMessage;
    QVariantList m_references;
    int m_selectedRow = -1;
    bool m_loading = false;
    bool m_operationActive = false;
    quint64 m_repositoryGeneration = 0;
    quint64 m_selectionGeneration = 0;
    quint64 m_diffGeneration = 0;
};

} // namespace GitNaga
