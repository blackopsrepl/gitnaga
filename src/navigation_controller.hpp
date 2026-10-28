#pragma once

#include "blame_model.hpp"
#include "file_history_model.hpp"
#include "search_controller.hpp"

#include <QObject>
#include <QPointer>
#include <QVariantList>

namespace GitNaga {

class RepositoryController;

// Everything that moves a person through a repository without changing it:
// the graph search, a file's history, its blame, graph-walking helpers and
// the command palette's ranking. It reads the repository controller's public
// state and asks it to select commits; it owns no repository state of its own.
class NavigationController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(CommitSearch *search READ search CONSTANT)
    Q_PROPERTY(FileHistoryModel *fileHistory READ fileHistory CONSTANT)
    Q_PROPERTY(BlameModel *blame READ blame CONSTANT)

public:
    explicit NavigationController(RepositoryController *repository, QObject *parent = nullptr);

    CommitSearch *search() { return &m_search; }
    FileHistoryModel *fileHistory() { return &m_fileHistory; }
    BlameModel *blame() { return &m_blame; }

    // `revision` is a commit oid, or the uncommitted-work oid (or empty) for
    // the worktree's own copy of the file.
    Q_INVOKABLE void showFileHistory(const QString &path, const QString &revision = {});
    Q_INVOKABLE void showBlame(const QString &path, const QString &revision = {});
    // The file selected in the review list, or empty.
    Q_INVOKABLE QString selectedPath() const;

    // Graph walking: the row of a commit's first parent, and of its nearest
    // child above it. -1 when there is none in the loaded graph.
    Q_INVOKABLE int parentRow(int row) const;
    Q_INVOKABLE int childRow(int row) const;
    // The loaded commit whose oid starts with `prefix` (four hex digits at
    // least), or empty when none or more than one does.
    Q_INVOKABLE QString resolveCommit(const QString &prefix) const;
    // Indices of `labels` that fuzzily match `needle`, best first.
    Q_INVOKABLE QVariantList rankCommands(const QStringList &labels, const QString &needle) const;

    static int commandScore(const QString &needle, const QString &label);

signals:
    void fileHistoryRequested();
    void blameRequested();

private:
    QPointer<RepositoryController> m_repository;
    CommitSearch m_search;
    FileHistoryModel m_fileHistory;
    BlameModel m_blame;
};

} // namespace GitNaga
