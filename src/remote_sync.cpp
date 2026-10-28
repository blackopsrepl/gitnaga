#include "remote_sync.hpp"

#include "git_sync.hpp"
#include "repository_controller.hpp"

#include <QGuiApplication>
#include <QSettings>

#include <memory>

namespace GitNaga {
namespace {

QString &askpassStorage()
{
    static QString program;
    return program;
}

const QStringList pullModes = { QStringLiteral("ff"), QStringLiteral("ff-only"), QStringLiteral("rebase") };

// Decided off the GUI thread, read back once the operation lands.
struct PushTarget {
    QString branch;
    QString remote;
    QStringList choices;
    bool rejected = false;
};

// git's refusal spans several lines of refspecs and hints; the footer gets
// the one sentence that matters, the dialog that follows offers the way out.
GitResult<QString> explainRefusal(GitResult<QString> result, PushTarget &target, const QString &remoteBranch,
                                  bool forceWithLease)
{
    if (result)
        return result;
    auto &message = result.error().message;
    if (forceWithLease && message.contains(QStringLiteral("stale info"))) {
        message = QObject::tr("Force push refused: %1/%2 moved since the last fetch. Fetch and look first.")
                      .arg(target.remote, remoteBranch);
    } else if (!forceWithLease && gitsync::isNonFastForward(message)) {
        target.rejected = true;
        message = QObject::tr("Push rejected: %1/%2 has commits %3 does not have")
                      .arg(target.remote, remoteBranch, target.branch);
    }
    return result;
}

} // namespace

RemoteSync::RemoteSync(RepositoryController *repository)
    : QObject(repository)
    , m_repository(repository)
{
    QSettings settings;
    m_autoFetch = settings.value(QStringLiteral("sync/autoFetch"), true).toBool();
    m_pullMode = settings.value(QStringLiteral("sync/pullMode"), QStringLiteral("ff")).toString();
    if (!pullModes.contains(m_pullMode))
        m_pullMode = QStringLiteral("ff");
    m_active = [] { return QGuiApplication::applicationState() == Qt::ApplicationActive; };
    // The clock starts when a repository opens: opening one is not a reason
    // to reach the network, ten minutes of looking at it is.
    m_sinceFetch.start();
    connect(repository, &RepositoryController::repositoryChanged, this, [this] { m_sinceFetch.restart(); });
    connect(&m_autoFetchTimer, &QTimer::timeout, this, &RemoteSync::autoFetchTick);
    setAutoFetchInterval(m_interval);
}

bool RemoteSync::autoFetch() const { return m_autoFetch; }
QString RemoteSync::pullMode() const { return m_pullMode; }

void RemoteSync::setAutoFetch(bool enabled)
{
    if (m_autoFetch == enabled)
        return;
    m_autoFetch = enabled;
    QSettings().setValue(QStringLiteral("sync/autoFetch"), enabled);
    emit settingsChanged();
}

void RemoteSync::setPullMode(const QString &mode)
{
    if (!pullModes.contains(mode) || m_pullMode == mode)
        return;
    m_pullMode = mode;
    QSettings().setValue(QStringLiteral("sync/pullMode"), mode);
    emit settingsChanged();
}

void RemoteSync::setAskpassProgram(const QString &program) { askpassStorage() = program; }
QString RemoteSync::askpassProgram() { return askpassStorage(); }

void RemoteSync::setAutoFetchInterval(int milliseconds)
{
    m_interval = milliseconds;
    // Checked often enough to fire close to the interval, rarely enough to
    // cost nothing.
    m_autoFetchTimer.start(std::min(60 * 1000, std::max(100, milliseconds / 4)));
}

void RemoteSync::setActivityProbe(std::function<bool()> probe) { m_active = std::move(probe); }

void RemoteSync::autoFetchTick()
{
    if (!m_autoFetch || m_repository->repositoryPath().isEmpty() || m_repository->busy() || !m_active())
        return;
    if (m_sinceFetch.elapsed() < m_interval)
        return;
    fetchQuietly(true);
}

void RemoteSync::fetch() { fetchQuietly(false); }

void RemoteSync::fetchQuietly(bool quiet)
{
    m_sinceFetch.restart();
    // A background fetch that worked is not news; one that failed is.
    m_repository->runAsyncOperation(quiet ? QString() : tr("Fetched all remotes"),
                                    [](const QString &worktree, const RunOptions &options) {
        if (options.progress)
            options.progress(QObject::tr("Fetching all remotes…"));
        return gitsync::fetchAll(worktree, gitsync::withCredentials(worktree, options, askpassProgram()));
    });
}

void RemoteSync::pull() { pullWith(m_pullMode); }

void RemoteSync::pullWith(const QString &mode)
{
    setPullMode(mode);
    const auto branch = m_repository->currentBranch();
    if (m_repository->detached() || branch.isEmpty()) {
        m_repository->reportFailure(tr("HEAD is detached: check out a branch to pull into"));
        return;
    }
    if (m_repository->upstream().isEmpty()) {
        m_repository->reportFailure(tr("%1 tracks no upstream branch").arg(branch));
        return;
    }
    const auto pullMode = gitsync::pullModeFromName(m_pullMode);
    m_repository->runAsyncOperation(tr("Pulled %1 into %2").arg(m_repository->upstream(), branch),
                                    [pullMode](const QString &worktree, const RunOptions &options) {
        if (options.progress)
            options.progress(QObject::tr("Pulling…"));
        return gitsync::pull(worktree, pullMode, gitsync::withCredentials(worktree, options, askpassProgram()));
    });
}

void RemoteSync::push()
{
    pushBranch(m_repository->currentBranch(), {}, false);
}

void RemoteSync::pushTo(const QString &remote)
{
    pushBranch(m_repository->currentBranch(), remote, false);
}

void RemoteSync::forcePushWithLease(const QString &branch, const QString &remote)
{
    pushBranch(branch, remote, true);
}

void RemoteSync::pushBranch(const QString &branch, const QString &chosenRemote, bool forceWithLease)
{
    if (branch.isEmpty() || m_repository->detached()) {
        m_repository->reportFailure(tr("HEAD is detached: check out a branch to push"));
        return;
    }
    auto target = std::make_shared<PushTarget>();
    target->branch = branch;
    m_repository->runAsyncOperation(forceWithLease ? tr("Force pushed %1 with lease").arg(branch)
                                                   : tr("Pushed %1").arg(branch),
                                    [target, chosenRemote, forceWithLease](const QString &worktree, const RunOptions &options) -> GitResult<QString> {
        gitsync::PushRequest request{ chosenRemote, target->branch, {}, false, forceWithLease };
        const auto tracking = gitsync::trackingOf(worktree, target->branch);
        if (chosenRemote.isEmpty() && !tracking.remote.isEmpty()) {
            request.remote = tracking.remote;
            request.target = tracking.branch;
        } else if (tracking.remote.isEmpty()) {
            // Publishing: the branch gets an upstream on the remote it lands on.
            request.setUpstream = true;
            if (request.remote.isEmpty()) {
                auto remotes = gitsync::listRemotes(worktree);
                if (!remotes)
                    return std::unexpected(remotes.error());
                if (remotes->isEmpty())
                    return std::unexpected(GitError{ QStringLiteral("push"), QObject::tr("This repository has no remote to push to") });
                request.remote = gitsync::defaultRemote(*remotes);
                if (request.remote.isEmpty()) {
                    target->choices = *remotes;
                    return std::unexpected(GitError{ QStringLiteral("push"), QObject::tr("Choose the remote to publish %1 to").arg(target->branch) });
                }
            }
        }
        target->remote = request.remote;
        if (options.progress)
            options.progress(QObject::tr("Pushing %1 to %2…").arg(request.branch, request.remote));
        auto result = gitsync::push(worktree, request, gitsync::withCredentials(worktree, options, askpassProgram()));
        return explainRefusal(std::move(result), *target, request.target.isEmpty() ? request.branch : request.target,
                              forceWithLease);
    }, [this, target](const QString &, const GitResult<QString> &result) {
        if (result)
            return;
        if (!target->choices.isEmpty())
            emit remoteChoiceNeeded(QString(), target->choices);
        else if (target->rejected)
            emit pushRejected(target->branch, target->remote, result.error().message);
    });
}

} // namespace GitNaga
