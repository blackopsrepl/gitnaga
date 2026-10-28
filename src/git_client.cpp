#include "git_client.hpp"

#include <algorithm>

#include "git_parse.hpp"

#include <QProcess>
#include <QProcessEnvironment>

namespace GitNaga {

using detail::decode;

namespace {

// Git paints progress with carriage returns; a person wants the last state of
// the line, not every intermediate percentage.
void reportProgress(QByteArray &pending, const QByteArray &chunk, const std::function<void(const QString &)> &progress)
{
    pending.append(chunk);
    qsizetype cut = -1;
    for (qsizetype index = pending.size() - 1; index >= 0; --index) {
        if (pending.at(index) == '\r' || pending.at(index) == '\n') {
            cut = index;
            break;
        }
    }
    if (cut < 0)
        return;
    const auto complete = pending.left(cut);
    pending.remove(0, cut + 1);
    const auto lines = complete.split('\n');
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        const auto parts = it->split('\r');
        for (auto part = parts.crbegin(); part != parts.crend(); ++part) {
            const auto text = decode(*part).trimmed();
            if (!text.isEmpty()) {
                progress(text);
                return;
            }
        }
    }
}

// One line names the failure, and the footer shows the first one: put git's
// own verdict first, ahead of chatter such as ssh's known-hosts notice, and
// say plainly when a remote refused the credentials. The rest follows.
QString leadWithVerdict(const QString &text)
{
    QStringList lines;
    for (auto line : text.split(QLatin1Char('\n'))) {
        line = line.trimmed();
        if (line.startsWith(QStringLiteral("Warning: Permanently added")))
            continue;
        lines.append(line);
    }
    while (!lines.isEmpty() && lines.constFirst().isEmpty())
        lines.removeFirst();
    const auto refused = std::find_if(lines.cbegin(), lines.cend(), [](const QString &line) {
        return line.contains(QStringLiteral("Authentication failed"))
            || line.contains(QStringLiteral("Permission denied (publickey"))
            || line.contains(QStringLiteral("Invalid username or password"));
    });
    if (refused != lines.cend()) {
        lines.prepend(refused->contains(QStringLiteral("publickey"))
                          ? QStringLiteral("The remote refused the ssh key: wrong passphrase, or the key is not authorized there")
                          : QStringLiteral("The remote refused the username or password"));
        return lines.join(QLatin1Char('\n'));
    }
    const auto verdict = std::find_if(lines.cbegin(), lines.cend(), [](const QString &line) {
        return line.startsWith(QStringLiteral("fatal:")) || line.startsWith(QStringLiteral("error:"));
    });
    if (verdict != lines.cend() && verdict != lines.cbegin()) {
        const auto first = *verdict;
        lines.erase(verdict);
        lines.prepend(first);
    }
    return lines.join(QLatin1Char('\n'));
}

QString failureMessage(const QByteArray &standardError, const QByteArray &standardOutput)
{
    // Conflicts and refusals often land on stdout (merge, cherry-pick), so a
    // silent stderr is not an empty explanation.
    auto message = decode(standardError).trimmed();
    if (message.isEmpty())
        message = decode(standardOutput).trimmed();
    return leadWithVerdict(message);
}

} // namespace

GitResult<QString> GitClient::mutate(const QString &worktree, const QStringList &arguments, const QString &operation,
                                     const RunOptions &options)
{
    auto output = run(worktree, arguments, operation, options);
    if (!output)
        return std::unexpected(output.error());
    return decode(*output).trimmed();
}

GitResult<QByteArray> GitClient::run(const QString &workingDirectory, const QStringList &arguments,
                                     const QString &operation, const QHash<QString, QString> &environment)
{
    RunOptions options;
    options.environment = environment;
    return run(workingDirectory, arguments, operation, options);
}

GitResult<QByteArray> GitClient::run(const QString &workingDirectory, const QStringList &arguments,
                                     const QString &operation, const RunOptions &options)
{
    QProcess process;
    process.setWorkingDirectory(workingDirectory);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    environment.insert(QStringLiteral("GIT_PAGER"), QStringLiteral("cat"));
    environment.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    environment.insert(QStringLiteral("GIT_EDITOR"), QStringLiteral("true"));
    environment.insert(QStringLiteral("GIT_SEQUENCE_EDITOR"), QStringLiteral("true"));
    // A viewer must never write the index behind the user's back: without
    // this, every `git status` refreshes the index, the watcher sees the
    // write, and the refresh it triggers runs `git status` again.
    environment.insert(QStringLiteral("GIT_OPTIONAL_LOCKS"), QStringLiteral("0"));
    for (auto it = options.environment.constBegin(); it != options.environment.constEnd(); ++it)
        environment.insert(it.key(), it.value());
    process.setProcessEnvironment(environment);
    process.setProgram(QStringLiteral("git"));
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();

    if (!process.waitForStarted(10000)) {
        // A program that cannot be started is not a command that failed: the
        // caller has to be able to tell them apart, so carry QProcess's own
        // FailedToStart code instead of the generic exit code.
        return std::unexpected(GitError{ operation, process.errorString(), QProcess::FailedToStart });
    }
    if (!options.input.isEmpty())
        process.write(options.input);
    process.closeWriteChannel();

    QByteArray standardOutput;
    QByteArray standardError;
    QByteArray pendingProgress;
    bool stoppedByReader = false;
    const auto drain = [&] {
        const auto out = process.readAllStandardOutput();
        if (!out.isEmpty()) {
            if (options.output) {
                if (!stoppedByReader && !options.output(out)) {
                    stoppedByReader = true;
                    process.kill();
                }
            } else {
                standardOutput.append(out);
            }
        }
        const auto err = process.readAllStandardError();
        if (!err.isEmpty()) {
            standardError.append(err);
            if (options.progress)
                reportProgress(pendingProgress, err, options.progress);
        }
    };

    while (process.state() != QProcess::NotRunning) {
        process.waitForFinished(50);
        drain();
        if (options.cancel && options.cancel->load()) {
            process.kill();
            process.waitForFinished();
            return std::unexpected(GitError{ operation, QStringLiteral("Cancelled") });
        }
    }
    drain();

    if (stoppedByReader)
        return standardOutput;
    if (process.exitStatus() != QProcess::NormalExit || !options.acceptedExitCodes.contains(process.exitCode()))
        return std::unexpected(GitError{ operation, failureMessage(standardError, standardOutput), process.exitCode() });
    return standardOutput;
}

} // namespace GitNaga
