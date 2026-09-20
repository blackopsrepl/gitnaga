#pragma once

#include "commit_model.hpp"
#include "diff_line_model.hpp"
#include "file_change_model.hpp"

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QUrl>

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
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)
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
    QString errorMessage() const;
    QString selectedOid() const;
    QString selectedSubject() const;
    QString selectedAuthor() const;
    QString selectedDate() const;
    QString selectedBody() const;

    Q_INVOKABLE void openRepository(const QUrl &url);
    Q_INVOKABLE void openRepositoryPath(const QString &path);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void selectCommit(int row);
    Q_INVOKABLE void selectFile(int row);

signals:
    void repositoryChanged();
    void loadingChanged();
    void errorChanged();
    void selectionChanged();

private:
    void setLoading(bool loading);
    void setError(QString message);
    void configureWatcher();
    void clearSelection();

    CommitModel m_commits;
    FileChangeModel m_changedFiles;
    DiffLineModel m_diffLines;
    QFileSystemWatcher m_watcher;
    QTimer m_refreshTimer;
    RepositorySnapshot m_repository;
    CommitDetails m_selected;
    QString m_requestedPath;
    QString m_error;
    bool m_loading = false;
    quint64 m_repositoryGeneration = 0;
    quint64 m_selectionGeneration = 0;
    quint64 m_diffGeneration = 0;
};

} // namespace GitNaga
