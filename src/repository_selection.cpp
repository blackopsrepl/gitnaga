#include "repository_controller.hpp"

#include "git_staging.hpp"

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
    m_diffKey.clear();

    if (oid == worktreeOid()) {
        m_selected = GitClient::inspectWorktree(m_repository.status).details;
        emit selectionChanged();
        emit revealRow(row);
        showWorktree();
        return;
    }
    m_unstagedFiles.replace({});
    m_stagedFiles.replace({});

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
    // A stash also lists the untracked files it saved in its third parent.
    const bool stash = isStash(oid);
    watcher->setFuture(QtConcurrent::run([worktree, oid, stash] {
        return stash ? staging::inspectStash(worktree, oid) : GitClient::inspectCommit(worktree, oid);
    }));
}

void RepositoryController::selectFile(int row)
{
    const auto *file = m_changedFiles.fileAt(row);
    m_selectedFile = file ? row : -1;
    const auto generation = ++m_diffGeneration;
    if (!file || m_selectedOid.isEmpty()) {
        m_selectedPath.clear();
        m_diffLines.clear();
        m_diffKey.clear();
        emit fileSelectionChanged();
        return;
    }
    m_selectedPath = file->path;
    const bool worktree = m_selectedOid == worktreeOid();
    // Work in progress lists the unstaged files first, then the staged ones,
    // and each side diffs against its own base.
    m_selectedSide = !worktree ? DiffSide::Commit
                   : row < m_unstagedFiles.count() ? DiffSide::Unstaged : DiffSide::Staged;
    const auto repository = m_repository.location.worktree;
    // A stash's untracked files live in its third parent, not in the stash.
    const bool stashedUntracked = !worktree && file->untracked && m_selected.parents.size() >= 3;
    const auto oid = worktree ? m_repository.refs.headOid
                   : stashedUntracked ? m_selected.parents.at(2) : m_selected.oid;
    const DiffRequest request{ repository, oid, file->path, file->oldPath, m_selectedSide,
                               file->untracked && worktree };
    const auto options = m_diffLines.options();
    const auto key = QString::number(static_cast<int>(m_selectedSide)) + QLatin1Char(':') + file->path;
    m_diffKey.clear();
    emit fileSelectionChanged();

    auto *watcher = new QFutureWatcher<GitResult<FileDiff>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation, key] {
        const auto result = watcher->result();
        watcher->deleteLater();
        if (generation != m_diffGeneration)
            return;
        if (!result) {
            setError(result.error().message);
            return;
        }
        m_diffLines.replace(*result);
        m_diffKey = key;
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
    m_unstagedFiles.replace({});
    m_stagedFiles.replace({});
    m_diffLines.clear();
    m_diffKey.clear();
    emit selectionChanged();
    emit fileSelectionChanged();
}

} // namespace GitNaga
