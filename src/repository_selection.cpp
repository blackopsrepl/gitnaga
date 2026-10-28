#include "repository_controller.hpp"

#include <QFutureWatcher>
#include <QtConcurrentRun>

namespace GitNaga {

void RepositoryController::selectCommit(int row)
{
    const auto *commit = m_commits.commitAt(row);
    if (!commit)
        return;
    inspect(commit->oid, row);
}

void RepositoryController::selectOid(const QString &oid)
{
    if (oid.isEmpty() || m_repository.location.worktree.isEmpty())
        return;
    // A ref whose commit is not in the graph (hidden, or still streaming in)
    // is still reviewable: the inspector does not need a row.
    inspect(oid, m_commits.rowOf(oid));
}

void RepositoryController::inspect(const QString &oid, int row)
{
    m_selectedRow = row;
    m_selectedOid = oid;
    m_selectedPath.clear();
    const auto generation = ++m_selectionGeneration;
    ++m_diffGeneration;
    m_diffLines.clear();

    if (oid == worktreeOid()) {
        const auto inspection = GitClient::inspectWorktree(m_repository.status);
        m_selected = inspection.details;
        m_changedFiles.replace(inspection.files);
        emit selectionChanged();
        emit revealRow(row);
        selectFile(inspection.files.isEmpty() ? -1 : 0);
        return;
    }

    emit selectionChanged();
    emit revealRow(row);
    const auto worktree = m_repository.location.worktree;
    auto *watcher = new QFutureWatcher<GitResult<CommitInspection>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation] {
        const auto result = watcher->result();
        watcher->deleteLater();
        if (generation != m_selectionGeneration)
            return;
        if (!result) {
            setError(result.error().message);
            return;
        }
        m_selected = result->details;
        m_changedFiles.replace(result->files);
        emit selectionChanged();
        selectFile(result->files.isEmpty() ? -1 : 0);
    });
    watcher->setFuture(QtConcurrent::run([worktree, oid] { return GitClient::inspectCommit(worktree, oid); }));
}

void RepositoryController::selectFile(int row)
{
    const auto *file = m_changedFiles.fileAt(row);
    m_selectedFile = file ? row : -1;
    emit fileSelectionChanged();
    const auto generation = ++m_diffGeneration;
    if (!file || m_selectedOid.isEmpty()) {
        m_selectedPath.clear();
        m_diffLines.clear();
        return;
    }
    m_selectedPath = file->path;
    const bool worktree = m_selectedOid == worktreeOid();
    const auto repository = m_repository.location.worktree;
    const auto oid = worktree ? m_repository.refs.headOid : m_selected.oid;
    const DiffRequest request{ repository, oid, file->path, file->oldPath,
                               worktree ? DiffSide::Worktree : DiffSide::Commit, file->untracked };
    const auto options = m_diffLines.options();

    auto *watcher = new QFutureWatcher<GitResult<FileDiff>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation] {
        const auto result = watcher->result();
        watcher->deleteLater();
        if (generation != m_diffGeneration)
            return;
        if (!result) {
            setError(result.error().message);
            return;
        }
        m_diffLines.replace(*result);
    });
    watcher->setFuture(QtConcurrent::run([request, options] { return GitClient::loadFileDiff(request, options); }));
}

void RepositoryController::restoreSelection()
{
    if (m_selectedOid.isEmpty())
        return;
    const int row = m_commits.rowOf(m_selectedOid);
    if (row == m_selectedRow)
        return;
    m_selectedRow = row;
    emit selectionChanged();
}

void RepositoryController::clearSelection()
{
    ++m_selectionGeneration;
    ++m_diffGeneration;
    m_selected = {};
    m_selectedOid.clear();
    m_selectedPath.clear();
    m_selectedRow = -1;
    m_selectedFile = -1;
    m_changedFiles.replace({});
    m_diffLines.clear();
    emit selectionChanged();
    emit fileSelectionChanged();
}

} // namespace GitNaga
