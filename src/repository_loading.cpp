#include "repository_controller.hpp"

#include "git_refs.hpp"
#include "recent_projects.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QThreadPool>
#include <QtConcurrentRun>

namespace GitNaga {

struct RepositoryController::SyncResult {
    GitError error;
    bool failed = false;
    RepositoryLocation location;
    QVector<Worktree> worktrees;
    RefSnapshot refs;
    WorktreeStatus status;
};

void RepositoryController::startSync(bool forceHistory)
{
    const QString path = m_requestedPath.isEmpty() ? m_repository.location.worktree : m_requestedPath;
    if (path.isEmpty())
        return;

    // The requested path is the user's latest intent, so it wins over the
    // repository already open; the generation counter discards whatever an
    // older sync returns, so a newer request supersedes it rather than being
    // dropped behind a loading guard.
    const bool opening = path != m_repository.location.worktree;
    const auto known = opening ? RepositoryLocation{} : m_repository.location;
    const auto generation = ++m_syncGeneration;
    if (opening || forceHistory)
        setLoading(true);

    auto *watcher = new QFutureWatcher<SyncResult>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation, forceHistory] {
        const auto result = watcher->result();
        watcher->deleteLater();
        if (generation != m_syncGeneration)
            return;
        setLoading(false);
        applySync(result, forceHistory);
    });
    watcher->setFuture(QtConcurrent::run([path, known] {
        SyncResult result;
        const auto fail = [&result](const GitError &error) {
            result.failed = true;
            result.error = error;
            return result;
        };
        if (known.worktree.isEmpty()) {
            auto location = GitClient::locate(path);
            if (!location)
                return fail(location.error());
            result.location = *location;
        } else {
            result.location = known;
        }
        auto worktrees = GitClient::listWorktrees(result.location.worktree);
        if (!worktrees)
            return fail(worktrees.error());
        result.worktrees = *worktrees;
        auto refs = GitClient::loadRefs(result.location);
        if (!refs)
            return fail(refs.error());
        result.refs = *refs;
        auto status = GitClient::loadStatus(result.location);
        if (!status)
            return fail(status.error());
        result.status = *status;
        return result;
    }));
}

void RepositoryController::applySync(const SyncResult &result, bool forceHistory)
{
    if (result.failed) {
        setError(result.error.message.isEmpty() ? result.error.operation : result.error.message);
        // A repository that failed to open leaves the previous one in place.
        m_requestedPath = m_repository.location.worktree;
        return;
    }
    setError({});

    const bool opened = result.location.worktree != m_repository.location.worktree;
    const bool refsMoved = opened || forceHistory || result.refs.fingerprint != m_repository.refs.fingerprint;
    const bool worktreesChanged = opened || result.worktrees.size() != m_repository.worktrees.size();

    if (opened) {
        if (m_historyCancel)
            m_historyCancel->store(true);
        clearSelection();
        m_commits.replace({});
        m_repository = {};
        m_repository.location = result.location;
        loadViewSettings();
        if (m_repository.location.worktree != m_recordedWorktree) {
            m_recordedWorktree = m_repository.location.worktree;
            RecentProjects::record(m_repository.location.worktree);
        }
    }
    m_repository.location = result.location;
    m_repository.worktrees = result.worktrees;
    m_repository.refs = result.refs;
    m_requestedPath = m_repository.location.worktree;

    if (refsMoved || worktreesChanged)
        configureWatcher();
    if (opened)
        emit repositoryChanged();
    rebuildReferences();
    applyStatus(result.status);
    if (refsMoved)
        loadHistory();
}

void RepositoryController::loadHistory()
{
    if (m_historyCancel)
        m_historyCancel->store(true);
    auto cancel = std::make_shared<std::atomic_bool>(false);
    m_historyCancel = cancel;
    const auto generation = ++m_historyGeneration;
    const auto worktree = m_repository.location.worktree;
    const auto revisions = GitClient::historyRevisions(m_repository.refs, m_view);
    auto decorations = gitrefs::decorations(m_repository.refs.refs, m_view.showInternal, m_view.hidden);
    // A detached HEAD has no branch to wear; it still deserves a chip.
    if (m_repository.refs.detached && !m_repository.refs.headOid.isEmpty()) {
        Ref head;
        head.kind = RefKind::Other;
        head.name = QStringLiteral("HEAD");
        head.fullName = QStringLiteral("HEAD");
        head.target = m_repository.refs.headOid;
        head.isHead = true;
        decorations[head.target].prepend(head);
    }
    // The first open streams so the top of a long history paints at once.
    // Every later reload keeps the old rows on screen and swaps in one step,
    // so the scroll position and selection survive the refresh.
    const bool progressive = m_commits.count() == (m_commits.hasWorkInProgress() ? 1 : 0);
    m_historyLoading = true;
    emit historyChanged();

    QThreadPool::globalInstance()->start([this, worktree, revisions, decorations, cancel, generation, progressive] {
        QVector<Commit> buffer;
        auto result = GitClient::streamHistory(
            worktree, revisions, decorations, 4000,
            [&](QVector<Commit> &&chunk) {
                if (!progressive) {
                    buffer.append(std::move(chunk));
                    return;
                }
                auto shared = std::make_shared<QVector<Commit>>(std::move(chunk));
                QMetaObject::invokeMethod(this, [this, generation, shared] {
                    if (generation != m_historyGeneration)
                        return;
                    m_commits.append(std::move(*shared));
                    m_commits.setWorkInProgress(m_commits.hasWorkInProgress(), m_repository.refs.headOid);
                    restoreSelection();
                    emit historyChanged();
                }, Qt::QueuedConnection);
            },
            cancel);
        const bool ok = result.has_value();
        const auto error = ok ? QString() : result.error().message;
        auto shared = std::make_shared<QVector<Commit>>(std::move(buffer));
        QMetaObject::invokeMethod(this, [this, generation, shared, ok, error, progressive] {
            if (generation != m_historyGeneration)
                return;
            if (ok && !progressive) {
                const bool workInProgress = m_commits.hasWorkInProgress();
                m_commits.replace(std::move(*shared));
                m_commits.setWorkInProgress(workInProgress, m_repository.refs.headOid);
            }
            m_historyLoading = false;
            if (!ok && error != QStringLiteral("Cancelled"))
                setError(error);
            restoreSelection();
            rebuildReferences();
            emit historyChanged();
        }, Qt::QueuedConnection);
    });
}

void RepositoryController::pollStatus()
{
    if (m_statusPolling || m_repository.location.worktree.isEmpty() || m_repository.location.isBare)
        return;
    if (auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
        application && application->applicationState() != Qt::ApplicationActive)
        return;
    m_statusPolling = true;
    const auto location = m_repository.location;
    const auto generation = m_syncGeneration;
    auto *watcher = new QFutureWatcher<GitResult<WorktreeStatus>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation] {
        auto result = watcher->result();
        watcher->deleteLater();
        m_statusPolling = false;
        // A sync that started meanwhile carries a newer status of its own.
        if (generation != m_syncGeneration || !result)
            return;
        if (result->fingerprint != m_repository.status.fingerprint)
            applyStatus(std::move(*result));
    });
    watcher->setFuture(QtConcurrent::run([location] { return GitClient::loadStatus(location); }));
}

void RepositoryController::applyStatus(WorktreeStatus status)
{
    const bool changed = status.fingerprint != m_repository.status.fingerprint;
    m_repository.status = std::move(status);
    m_commits.setWorkInProgress(!m_repository.status.files.isEmpty(), m_repository.refs.headOid);
    if (m_selectedOid == worktreeOid()) {
        if (m_repository.status.files.isEmpty()) {
            clearSelection();
        } else if (changed) {
            // Keep the file the user is reading; reload it, since its diff is
            // what just changed.
            const auto path = m_selectedPath;
            const auto inspection = GitClient::inspectWorktree(m_repository.status);
            m_changedFiles.replace(inspection.files);
            int row = 0;
            for (qsizetype index = 0; index < inspection.files.size(); ++index) {
                if (inspection.files.at(index).path == path)
                    row = static_cast<int>(index);
            }
            selectFile(row);
        }
    }
    restoreSelection();
    emit statusChanged();
}

void RepositoryController::configureWatcher()
{
    if (!m_watcher.files().isEmpty())
        m_watcher.removePaths(m_watcher.files());
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());

    const auto &location = m_repository.location;
    QStringList paths;
    const QStringList files = {
        QDir(location.gitDirectory).filePath(QStringLiteral("HEAD")),
        QDir(location.gitDirectory).filePath(QStringLiteral("index")),
        QDir(location.commonDirectory).filePath(QStringLiteral("packed-refs")),
    };
    for (const auto &file : files) {
        if (QFileInfo::exists(file))
            paths.append(file);
    }
    // The git directory itself shows MERGE_HEAD, rebase state and friends
    // appearing and disappearing.
    paths.append(location.gitDirectory);
    if (location.commonDirectory != location.gitDirectory && QFileInfo::exists(location.commonDirectory))
        paths.append(location.commonDirectory);
    for (const auto &root : { QStringLiteral("worktrees"), QStringLiteral("refs") }) {
        const auto directory = QDir(location.commonDirectory).filePath(root);
        if (!QFileInfo::exists(directory))
            continue;
        paths.append(directory);
        QDirIterator iterator(directory, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (iterator.hasNext())
            paths.append(iterator.next());
    }
    m_watcher.addPaths(paths);
}

} // namespace GitNaga
