#include "repository_controller.hpp"

#include "history_editor.hpp"
#include "operation_controller.hpp"
#include "project_browser.hpp"
#include "worktree_manager.hpp"
#include "remote_sync.hpp"
#include "perf_trace.hpp"

#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QLocale>
#include <QThreadPool>
#include <QtConcurrentRun>

namespace GitNaga {

RepositoryController::RepositoryController(QObject *parent)
    : QObject(parent)
    , m_sync(new RemoteSync(this))
    , m_history(new HistoryEditor(this))
    , m_operation(new OperationController(this))
    , m_worktreeManager(new WorktreeManager(this))
    , m_projects(new ProjectBrowser(this))
{
    m_syncTimer.setSingleShot(true);
    m_syncTimer.setInterval(150);
    m_messageTimer.setSingleShot(true);
    m_messageTimer.setInterval(6000);
    // Editing a file touches nothing under .git, so the worktree is polled
    // while the window is in use; `git status` with optional locks off is
    // read-only and cheap enough for that.
    m_statusTimer.setInterval(2000);
    connect(&m_syncTimer, &QTimer::timeout, this, &RepositoryController::synchronize);
    connect(&m_statusTimer, &QTimer::timeout, this, &RepositoryController::pollStatus);
    connect(&m_messageTimer, &QTimer::timeout, this, [this] { setOperationMessage({}, m_lastOperationOk); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_syncTimer.start(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_syncTimer.start(); });
    if (auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        connect(application, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
            if (state == Qt::ApplicationActive)
                pollStatus();
        });
    }
    m_statusTimer.start();
}

RepositoryController::~RepositoryController()
{
    // Workers post their results back to this object, so none may outlive it.
    if (m_historyCancel)
        m_historyCancel->store(true);
    if (m_operationCancel)
        m_operationCancel->store(true);
    QThreadPool::globalInstance()->waitForDone();
}

CommitModel *RepositoryController::commits() { return &m_commits; }
FileChangeModel *RepositoryController::changedFiles() { return &m_changedFiles; }
DiffLineModel *RepositoryController::diffLines() { return &m_diffLines; }
QString RepositoryController::repositoryPath() const { return m_repository.location.worktree; }
QString RepositoryController::repositoryName() const { return QFileInfo(m_repository.location.worktree).fileName(); }
QString RepositoryController::currentBranch() const { return m_repository.refs.currentBranch; }
QString RepositoryController::headOid() const { return m_repository.refs.headOid; }
bool RepositoryController::detached() const { return m_repository.refs.detached && !m_repository.refs.headOid.isEmpty(); }

int RepositoryController::commitCount() const
{
    return m_commits.count() - (m_commits.hasWorkInProgress() ? 1 : 0);
}

bool RepositoryController::historyLoading() const { return m_historyLoading; }
QString RepositoryController::workInProgressOid() const { return m_commits.hasWorkInProgress() ? worktreeOid() : QString(); }
int RepositoryController::uncommittedCount() const { return static_cast<int>(m_repository.status.files.size()); }
QString RepositoryController::operationInProgress() const { return m_repository.status.operation; }
QString RepositoryController::upstream() const { return m_repository.status.upstream; }
int RepositoryController::ahead() const { return m_repository.status.ahead; }
int RepositoryController::behind() const { return m_repository.status.behind; }
bool RepositoryController::loading() const { return m_loading; }
bool RepositoryController::busy() const { return m_operationActive || !m_operations.isEmpty(); }
int RepositoryController::pendingOperations() const { return static_cast<int>(m_operations.size()) + (m_operationActive ? 1 : 0); }
QString RepositoryController::operationProgress() const { return m_operationProgress; }
QString RepositoryController::errorMessage() const { return m_error; }
QString RepositoryController::operationMessage() const { return m_operationMessage; }
bool RepositoryController::lastOperationOk() const { return m_lastOperationOk; }
int RepositoryController::selectedRow() const { return m_selectedRow; }
int RepositoryController::selectedFile() const { return m_selectedFile; }
QVariantList RepositoryController::references() const { return m_references; }
QString RepositoryController::selectedOid() const { return m_selectedOid; }

QVariantMap RepositoryController::selectedCommit() const
{
    return {
        { QStringLiteral("subject"), m_selected.subject },
        { QStringLiteral("author"), m_selected.author },
        { QStringLiteral("date"), m_selected.authoredAt.isValid()
                                      ? QLocale().toString(m_selected.authoredAt, QLocale::LongFormat) : QString() },
        { QStringLiteral("body"), m_selected.body },
        { QStringLiteral("parents"), m_selected.parents },
    };
}

void RepositoryController::openRepository(const QUrl &url)
{
    openRepositoryPath(url.toLocalFile());
}

void RepositoryController::openRepositoryPath(const QString &path)
{
    perf::restart();
    perf::mark("open requested");
    m_requestedPath = QDir::cleanPath(path);
    startSync(true);
}

void RepositoryController::refresh()
{
    startSync(true);
}

void RepositoryController::synchronize()
{
    startSync(false);
}

void RepositoryController::cancelOperation()
{
    if (m_operationCancel)
        m_operationCancel->store(true);
    // Queued work has not started; cancelling means it never will.
    if (!m_operations.isEmpty()) {
        m_operations.clear();
        emit busyChanged();
    }
}

void RepositoryController::runOperation(const QString &operation, const QStringList &arguments, const QString &successMessage)
{
    runAsyncOperation(successMessage, [arguments, operation](const QString &worktree, const RunOptions &options) {
        return GitClient::mutate(worktree, arguments, operation, options);
    });
}

void RepositoryController::runAsyncOperation(const QString &successMessage, OperationWork work, OperationResult result)
{
    if (m_repository.location.worktree.isEmpty())
        return;
    m_operations.enqueue({ successMessage, std::move(work), std::move(result) });
    emit busyChanged();
    startNextOperation();
}

void RepositoryController::startNextOperation()
{
    if (m_operationActive || m_operations.isEmpty())
        return;
    auto operation = m_operations.dequeue();
    m_operationActive = true;
    m_operationCancel = std::make_shared<std::atomic_bool>(false);
    emit busyChanged();

    RunOptions options;
    options.cancel = m_operationCancel;
    options.progress = [this](const QString &line) {
        QMetaObject::invokeMethod(this, [this, line] { setOperationProgress(line); }, Qt::QueuedConnection);
    };

    const auto worktree = m_repository.location.worktree;
    auto *watcher = new QFutureWatcher<GitResult<QString>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this,
            [this, watcher, worktree, successMessage = operation.successMessage, result = operation.result] {
        const auto outcome = watcher->result();
        watcher->deleteLater();
        m_operationActive = false;
        m_operationCancel.reset();
        setOperationProgress({});
        if (result)
            result(worktree, outcome);
        if (!outcome) {
            const auto message = outcome.error().message.isEmpty() ? outcome.error().operation : outcome.error().message;
            setOperationMessage(message, false);
            emit operationFinished(false, message);
        } else {
            setOperationMessage(successMessage, true);
            emit operationFinished(true, successMessage);
        }
        emit busyChanged();
        synchronize();
        startNextOperation();
    });
    watcher->setFuture(QtConcurrent::run([worktree, options, work = std::move(operation.work)] {
        return work(worktree, options);
    }));
}

void RepositoryController::reportFailure(const QString &message)
{
    setOperationMessage(message, false);
    emit operationFinished(false, message);
}

void RepositoryController::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

void RepositoryController::setError(QString message)
{
    if (m_error == message)
        return;
    m_error = std::move(message);
    emit errorChanged();
}

void RepositoryController::setOperationMessage(QString message, bool ok)
{
    if (m_operationMessage == message && m_lastOperationOk == ok)
        return;
    m_operationMessage = std::move(message);
    m_lastOperationOk = ok;
    if (!m_operationMessage.isEmpty())
        m_messageTimer.start();
    emit operationMessageChanged();
}

void RepositoryController::setOperationProgress(QString progress)
{
    if (m_operationProgress == progress)
        return;
    m_operationProgress = std::move(progress);
    emit operationProgressChanged();
}

} // namespace GitNaga
