#include "repository_controller.hpp"

#include "git_staging.hpp"
#include "patch_builder.hpp"

#include <algorithm>

namespace GitNaga {

namespace {

// The row to show after the lists were rebuilt: the same file on the same
// side, else the same file on the other side (it was just staged or
// unstaged whole), else whatever now sits where the user was.
int worktreeRow(const QVector<FileChange> &all, int unstagedCount, const QString &path, DiffSide side, int previous)
{
    if (all.isEmpty())
        return -1;
    int other = -1;
    for (int row = 0; row < all.size(); ++row) {
        if (all.at(row).path != path)
            continue;
        const auto rowSide = row < unstagedCount ? DiffSide::Unstaged : DiffSide::Staged;
        if (rowSide == side)
            return row;
        other = row;
    }
    if (other >= 0)
        return other;
    return std::clamp(previous, 0, static_cast<int>(all.size()) - 1);
}

QStringList pathsOf(const FileChange &file)
{
    QStringList paths{ file.path };
    if (!file.oldPath.isEmpty())
        paths.append(file.oldPath);
    return paths;
}

} // namespace

QString RepositoryController::wipSide() const
{
    if (m_selectedOid != worktreeOid() || m_selectedFile < 0)
        return {};
    return m_selectedSide == DiffSide::Staged ? QStringLiteral("staged") : QStringLiteral("unstaged");
}

int RepositoryController::stagedCount() const
{
    return static_cast<int>(std::ranges::count_if(m_repository.status.files, &FileChange::staged));
}

bool RepositoryController::isStash(const QString &oid) const
{
    return std::ranges::any_of(m_repository.refs.stashes, [&oid](const StashEntry &stash) { return stash.oid == oid; });
}

void RepositoryController::showWorktree()
{
    QVector<FileChange> unstaged;
    QVector<FileChange> staged;
    staging::splitStatus(m_repository.status.files, &unstaged, &staged);
    const auto unstagedCount = static_cast<int>(unstaged.size());
    auto all = unstaged;
    all.append(staged);
    const int row = m_selectedPath.isEmpty() ? 0
                  : worktreeRow(all, unstagedCount, m_selectedPath, m_selectedSide, m_selectedFile);
    m_unstagedFiles.replace(std::move(unstaged));
    m_stagedFiles.replace(std::move(staged));
    m_changedFiles.replace(std::move(all));
    selectFile(m_changedFiles.count() > 0 ? row : -1);
}

void RepositoryController::selectWorktreeFile(const QString &side, int row)
{
    if (m_selectedOid != worktreeOid() || row < 0)
        return;
    selectFile(side == QStringLiteral("staged") ? m_unstagedFiles.count() + row : row);
}

void RepositoryController::runStaging(const QString &successMessage, OperationWork work)
{
    // A worktree-only change can leave `git status` byte-identical, so the
    // shown diff is reloaded here rather than left to the status refresh.
    runAsyncOperation(successMessage, std::move(work), [this](const QString &, const GitResult<QString> &) {
        if (m_selectedOid == worktreeOid())
            selectFile(m_selectedFile);
    });
}

void RepositoryController::stageFile(int row)
{
    const auto *file = m_unstagedFiles.fileAt(row);
    if (!file)
        return;
    const QStringList paths{ file->path };
    runStaging(tr("Staged %1").arg(file->path), [paths](const QString &worktree, const RunOptions &options) {
        return staging::stage(worktree, paths, options);
    });
}

void RepositoryController::unstageFile(int row)
{
    const auto *file = m_stagedFiles.fileAt(row);
    if (!file)
        return;
    const auto paths = pathsOf(*file);
    runStaging(tr("Unstaged %1").arg(file->path), [paths](const QString &worktree, const RunOptions &options) {
        return staging::unstage(worktree, paths, options);
    });
}

void RepositoryController::discardFile(int row)
{
    const auto *file = m_unstagedFiles.fileAt(row);
    if (!file)
        return;
    const auto tracked = file->untracked ? QStringList{} : QStringList{ file->path };
    const auto untracked = file->untracked ? QStringList{ file->path } : QStringList{};
    runStaging(file->untracked ? tr("Deleted %1").arg(file->path) : tr("Discarded changes to %1").arg(file->path),
               [tracked, untracked](const QString &worktree, const RunOptions &options) {
                   return staging::discard(worktree, tracked, untracked, options);
               });
}

void RepositoryController::stageAll()
{
    runStaging(tr("Staged all changes"), [](const QString &worktree, const RunOptions &options) {
        return staging::stage(worktree, { QStringLiteral(":/") }, options);
    });
}

void RepositoryController::unstageAll()
{
    runStaging(tr("Unstaged all changes"), [](const QString &worktree, const RunOptions &options) {
        return staging::unstage(worktree, { QStringLiteral(":/") }, options);
    });
}

void RepositoryController::discardAll()
{
    QStringList tracked;
    QStringList untracked;
    for (const auto &file : m_unstagedFiles.files()) {
        if (file.untracked)
            untracked.append(file.path);
        else if (!file.conflicted)
            tracked.append(file.path);
    }
    if (tracked.isEmpty() && untracked.isEmpty())
        return;
    runStaging(tr("Discarded all unstaged changes"), [tracked, untracked](const QString &worktree, const RunOptions &options) {
        return staging::discard(worktree, tracked, untracked, options);
    });
}

void RepositoryController::applyHunk(int hunk, const QString &action)
{
    applyPatchRows(patch::hunkChanges(m_diffLines.lines(), m_diffLines.sourceOfHunk(hunk)), action, tr("a hunk"));
}

void RepositoryController::applyLines(const QVariantList &rows, const QString &action)
{
    QSet<int> selected;
    for (const auto &row : rows)
        if (const int source = m_diffLines.sourceOfRow(row.toInt()); source >= 0)
            selected.insert(source);
    applyPatchRows(selected, action, selected.size() == 1 ? tr("a line") : tr("%1 lines").arg(selected.size()));
}

void RepositoryController::applyPatchRows(const QSet<int> &rows, const QString &action, const QString &what)
{
    const auto expectedKey = QString::number(static_cast<int>(m_selectedSide)) + QLatin1Char(':') + m_selectedPath;
    const bool unstagedSide = m_selectedSide == DiffSide::Unstaged;
    const bool allowed = action == QStringLiteral("unstage") ? m_selectedSide == DiffSide::Staged
                       : (action == QStringLiteral("stage") || action == QStringLiteral("discard")) && unstagedSide;
    if (m_selectedOid != worktreeOid() || !allowed || m_diffKey != expectedKey) {
        reportFailure(tr("The diff is still loading; try again"));
        return;
    }
    // A whitespace-blind diff's context does not match the file, so git
    // would refuse the patch.
    if (m_diffLines.ignoreWhitespace()) {
        reportFailure(tr("Turn off Ignore whitespace to stage parts of a file"));
        return;
    }
    const bool reverse = action != QStringLiteral("stage");
    const bool cached = action != QStringLiteral("discard");
    const auto built = patch::build(m_diffLines.lines(), rows,
                                    reverse ? patch::Direction::Reverse : patch::Direction::Forward);
    if (!built) {
        reportFailure(built.error());
        return;
    }
    const auto verb = action == QStringLiteral("stage") ? tr("Staged") : action == QStringLiteral("unstage")
                    ? tr("Unstaged") : tr("Discarded");
    runStaging(tr("%1 %2 of %3").arg(verb, what, m_selectedPath),
               [patch = *built, cached, reverse](const QString &worktree, const RunOptions &options) {
                   return staging::applyPatch(worktree, patch, cached, reverse, options);
               });
}

} // namespace GitNaga
