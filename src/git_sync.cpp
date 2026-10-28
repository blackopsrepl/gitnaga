#include "git_sync.hpp"

#include <QProcessEnvironment>

namespace GitNaga::gitsync {
namespace {

QString name(const char *text)
{
    return QString::fromLatin1(text);
}

QStringList withProgress(QStringList arguments, const RunOptions &options)
{
    // Git only paints progress on a terminal unless asked; the footer is that
    // terminal here.
    if (options.progress)
        arguments.insert(1, QStringLiteral("--progress"));
    return arguments;
}

bool userHasAskpass(const QString &worktree)
{
    const auto environment = QProcessEnvironment::systemEnvironment();
    if (!environment.value(QStringLiteral("GIT_ASKPASS")).isEmpty())
        return true;
    RunOptions options;
    options.acceptedExitCodes = { 0, 1 };
    const auto configured = GitClient::mutate(worktree, { QStringLiteral("config"), QStringLiteral("--get"),
                                                          QStringLiteral("core.askPass") },
                                              QStringLiteral("read askpass"), options);
    return configured && !configured->isEmpty();
}

} // namespace

PullMode pullModeFromName(const QString &mode)
{
    if (mode == name("ff-only"))
        return PullMode::FastForwardOnly;
    if (mode == name("rebase"))
        return PullMode::Rebase;
    return PullMode::FastForwardOrMerge;
}

QString pullModeName(PullMode mode)
{
    switch (mode) {
    case PullMode::FastForwardOnly: return name("ff-only");
    case PullMode::Rebase: return name("rebase");
    case PullMode::FastForwardOrMerge: break;
    }
    return name("ff");
}

RunOptions withCredentials(const QString &worktree, RunOptions options, const QString &askpassProgram)
{
    if (askpassProgram.isEmpty() || userHasAskpass(worktree))
        return options;
    options.environment.insert(QStringLiteral("GIT_ASKPASS"), askpassProgram);
    // ssh asks for passphrases and host keys through its own variable; force
    // makes it use the dialog even when no X display is advertised.
    if (QProcessEnvironment::systemEnvironment().value(QStringLiteral("SSH_ASKPASS")).isEmpty()) {
        options.environment.insert(QStringLiteral("SSH_ASKPASS"), askpassProgram);
        options.environment.insert(QStringLiteral("SSH_ASKPASS_REQUIRE"), QStringLiteral("force"));
    }
    // git and ssh call the program with the prompt as the only argument, so
    // the marker is how the binary knows it was started to answer one.
    options.environment.insert(QStringLiteral("GITNAGA_ASKPASS"), QStringLiteral("1"));
    return options;
}

GitResult<QStringList> listRemotes(const QString &worktree)
{
    auto output = GitClient::mutate(worktree, { QStringLiteral("remote") }, QStringLiteral("list remotes"));
    if (!output)
        return std::unexpected(output.error());
    return output->split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

Tracking trackingOf(const QString &worktree, const QString &branch)
{
    RunOptions options;
    options.acceptedExitCodes = { 0, 1 };
    const auto read = [&](const QString &key) {
        auto value = GitClient::mutate(worktree, { QStringLiteral("config"), QStringLiteral("--get"),
                                                   QStringLiteral("branch.%1.%2").arg(branch, key) },
                                       QStringLiteral("read upstream"), options);
        return value ? *value : QString();
    };
    Tracking tracking{ read(QStringLiteral("remote")), read(QStringLiteral("merge")) };
    if (tracking.remote.isEmpty() || tracking.branch.isEmpty())
        return {};
    tracking.branch.remove(0, tracking.branch.startsWith(QStringLiteral("refs/heads/")) ? 11 : 0);
    return tracking;
}

QString defaultRemote(const QStringList &remotes)
{
    if (remotes.size() == 1)
        return remotes.constFirst();
    if (remotes.contains(name("origin")))
        return name("origin");
    return {};
}

GitResult<QString> fetchAll(const QString &worktree, const RunOptions &options)
{
    return GitClient::mutate(worktree,
                             withProgress({ QStringLiteral("fetch"), QStringLiteral("--all"), QStringLiteral("--prune") },
                                          options),
                             QStringLiteral("fetch"), options);
}

GitResult<QString> pull(const QString &worktree, PullMode mode, const RunOptions &options)
{
    QStringList arguments{ QStringLiteral("pull") };
    switch (mode) {
    case PullMode::FastForwardOrMerge: arguments << QStringLiteral("--no-rebase"); break;
    case PullMode::FastForwardOnly: arguments << QStringLiteral("--ff-only"); break;
    case PullMode::Rebase: arguments << QStringLiteral("--rebase"); break;
    }
    return GitClient::mutate(worktree, withProgress(arguments, options), QStringLiteral("pull"), options);
}

GitResult<QString> push(const QString &worktree, const PushRequest &request, const RunOptions &options)
{
    if (request.remote.isEmpty() || request.branch.isEmpty())
        return std::unexpected(GitError{ QStringLiteral("push"), QStringLiteral("Nothing to push to") });
    QStringList arguments{ QStringLiteral("push") };
    if (request.setUpstream)
        arguments << QStringLiteral("--set-upstream");
    if (request.forceWithLease)
        arguments << QStringLiteral("--force-with-lease");
    // Spelled out on both sides so a branch that shares its name with a tag,
    // or a push.default of "nothing", cannot redirect the push.
    const auto target = request.target.isEmpty() ? request.branch : request.target;
    arguments << request.remote << QStringLiteral("refs/heads/%1:refs/heads/%2").arg(request.branch, target);
    return GitClient::mutate(worktree, withProgress(arguments, options), QStringLiteral("push"), options);
}

GitResult<QString> pushTag(const QString &worktree, const QString &remote, const QString &tag, const RunOptions &options)
{
    if (remote.isEmpty() || tag.isEmpty())
        return std::unexpected(GitError{ QStringLiteral("push tag"), QStringLiteral("Nothing to push to") });
    return GitClient::mutate(worktree,
                             withProgress({ QStringLiteral("push"), remote,
                                            QStringLiteral("refs/tags/%1:refs/tags/%1").arg(tag) }, options),
                             QStringLiteral("push tag"), options);
}

bool isNonFastForward(const QString &message)
{
    if (!message.contains(QStringLiteral("[rejected]")))
        return false;
    // "non-fast-forward" when this repository already saw the new commits,
    // "fetch first" when it did not.
    return message.contains(QStringLiteral("non-fast-forward")) || message.contains(QStringLiteral("fetch first"));
}

GitResult<QString> setUpstream(const QString &worktree, const QString &branch, const QString &upstream)
{
    return GitClient::mutate(worktree, { QStringLiteral("branch"), QStringLiteral("--set-upstream-to=") + upstream, branch },
                             QStringLiteral("set upstream"));
}

GitResult<QString> unsetUpstream(const QString &worktree, const QString &branch)
{
    return GitClient::mutate(worktree, { QStringLiteral("branch"), QStringLiteral("--unset-upstream"), branch },
                             QStringLiteral("unset upstream"));
}

} // namespace GitNaga::gitsync
