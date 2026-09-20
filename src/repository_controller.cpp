#include "repository_controller.hpp"

#include "git_client.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLocale>
#include <QtConcurrentRun>


namespace GitNaga {

RepositoryController::RepositoryController(QObject *parent)
    : QObject(parent)
{
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(150);
    m_messageTimer.setSingleShot(true);
    m_messageTimer.setInterval(6000);
    connect(&m_refreshTimer, &QTimer::timeout, this, &RepositoryController::refresh);
    connect(&m_messageTimer, &QTimer::timeout, this, [this] { setOperationMessage({}); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_refreshTimer.start(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_refreshTimer.start(); });
}

CommitModel *RepositoryController::commits() { return &m_commits; }
FileChangeModel *RepositoryController::changedFiles() { return &m_changedFiles; }
DiffLineModel *RepositoryController::diffLines() { return &m_diffLines; }
QString RepositoryController::repositoryPath() const { return m_repository.worktree; }
QString RepositoryController::repositoryName() const { return QFileInfo(m_repository.worktree).fileName(); }
QString RepositoryController::currentBranch() const { return m_repository.currentBranch; }
bool RepositoryController::loading() const { return m_loading; }
bool RepositoryController::busy() const { return m_loading || m_operationActive; }
QString RepositoryController::errorMessage() const { return m_error; }
QString RepositoryController::operationMessage() const { return m_operationMessage; }
int RepositoryController::selectedRow() const { return m_selectedRow; }
QVariantList RepositoryController::references() const { return m_references; }
QString RepositoryController::selectedOid() const { return m_selected.oid; }
QString RepositoryController::selectedSubject() const { return m_selected.subject; }
QString RepositoryController::selectedAuthor() const { return m_selected.author; }
QString RepositoryController::selectedDate() const { return QLocale().toString(m_selected.authoredAt, QLocale::LongFormat); }
QString RepositoryController::selectedBody() const { return m_selected.body; }

void RepositoryController::openRepository(const QUrl &url)
{
    openRepositoryPath(url.toLocalFile());
}

void RepositoryController::openRepositoryPath(const QString &path)
{
    m_requestedPath = QDir::cleanPath(path);
    refresh();
}

void RepositoryController::refresh()
{
    const QString path = m_repository.worktree.isEmpty() ? m_requestedPath : m_repository.worktree;
    if (path.isEmpty() || m_loading)
        return;

    const auto generation = ++m_repositoryGeneration;
    setLoading(true);
    setError({});

    auto *watcher = new QFutureWatcher<GitResult<RepositorySnapshot>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation] {
        const auto result = watcher->result();
        watcher->deleteLater();
        if (generation != m_repositoryGeneration)
            return;
        setLoading(false);
        if (!result) {
            setError(result.error().message);
            return;
        }
        m_repository = *result;
        m_requestedPath = m_repository.worktree;
        m_commits.replace(m_repository.commits);
        clearSelection();
        configureWatcher();
        rebuildReferences();
        emit repositoryChanged();
    });
    watcher->setFuture(QtConcurrent::run([path] { return GitClient::loadRepository(path); }));
}

void RepositoryController::selectCommit(int row)
{
    const auto *commit = m_commits.commitAt(row);
    if (!commit || m_repository.worktree.isEmpty())
        return;

    m_selectedRow = row;
    emit selectionChanged();

    const auto generation = ++m_selectionGeneration;
    ++m_diffGeneration;
    const auto worktree = m_repository.worktree;
    const auto oid = commit->oid;
    m_diffLines.clear();

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
        if (!result->files.isEmpty())
            selectFile(0);
    });
    watcher->setFuture(QtConcurrent::run([worktree, oid] { return GitClient::inspectCommit(worktree, oid); }));
}

void RepositoryController::selectFile(int row)
{
    const auto *file = m_changedFiles.fileAt(row);
    if (!file || m_selected.oid.isEmpty())
        return;
    const auto generation = ++m_diffGeneration;
    const auto worktree = m_repository.worktree;
    const auto oid = m_selected.oid;
    const auto path = file->path;

    auto *watcher = new QFutureWatcher<GitResult<QVector<DiffLine>>>(this);
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
    watcher->setFuture(QtConcurrent::run([worktree, oid, path] { return GitClient::loadDiff(worktree, oid, path); }));
}

void RepositoryController::runOperation(const QString &operation, const QStringList &arguments, const QString &successMessage)
{
    if (m_repository.worktree.isEmpty() || m_operationActive)
        return;
    m_operationActive = true;
    emit busyChanged();

    const auto worktree = m_repository.worktree;
    auto *watcher = new QFutureWatcher<GitResult<QString>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, successMessage] {
        const auto result = watcher->result();
        watcher->deleteLater();
        m_operationActive = false;
        emit busyChanged();
        if (!result) {
            const auto message = result.error().message.isEmpty() ? result.error().operation : result.error().message;
            setOperationMessage(message);
            emit operationFinished(false, message);
            return;
        }
        setOperationMessage(successMessage);
        emit operationFinished(true, successMessage);
        refresh();
    });
    watcher->setFuture(QtConcurrent::run([worktree, arguments, operation] {
        return GitClient::mutate(worktree, arguments, operation);
    }));
}

void RepositoryController::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
    emit busyChanged();
}

void RepositoryController::setError(QString message)
{
    if (errorMessage() == message)
        return;
    m_error = std::move(message);
    emit errorChanged();
}

void RepositoryController::setOperationMessage(QString message)
{
    if (m_operationMessage == message)
        return;
    m_operationMessage = std::move(message);
    if (!m_operationMessage.isEmpty())
        m_messageTimer.start();
    emit operationMessageChanged();
}

void RepositoryController::configureWatcher()
{
    if (!m_watcher.files().isEmpty())
        m_watcher.removePaths(m_watcher.files());
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());

    QStringList paths;
    const QStringList files = {
        QDir(m_repository.gitDirectory).filePath(QStringLiteral("HEAD")),
        QDir(m_repository.gitDirectory).filePath(QStringLiteral("index")),
        QDir(m_repository.commonDirectory).filePath(QStringLiteral("packed-refs")),
    };
    for (const auto &file : files) {
        if (QFileInfo::exists(file))
            paths.append(file);
    }

    const auto refs = QDir(m_repository.commonDirectory).filePath(QStringLiteral("refs"));
    if (QFileInfo::exists(refs)) {
        paths.append(refs);
        QDirIterator iterator(refs, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (iterator.hasNext())
            paths.append(iterator.next());
    }
    if (!paths.isEmpty())
        m_watcher.addPaths(paths);
}

void RepositoryController::clearSelection()
{
    ++m_selectionGeneration;
    ++m_diffGeneration;
    m_selected = {};
    m_selectedRow = -1;
    m_changedFiles.replace({});
    m_diffLines.clear();
    emit selectionChanged();
}

} // namespace GitNaga
