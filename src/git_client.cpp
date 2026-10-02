#include "git_client.hpp"

#include "git_parse.hpp"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>

namespace GitNaga {

using detail::decode;

namespace {

GitError processError(const QString &operation, QProcess &process)
{
    return GitError{ operation, decode(process.readAllStandardError()).trimmed(), process.exitCode() };
}

} // namespace

GitResult<QString> GitClient::mutate(const QString &worktree, const QStringList &arguments, const QString &operation)
{
    auto output = run(worktree, arguments, operation);
    if (!output)
        return std::unexpected(output.error());
    return decode(*output).trimmed();
}

GitResult<QByteArray> GitClient::run(const QString &workingDirectory, const QStringList &arguments,
                                     const QString &operation, const QHash<QString, QString> &extraEnvironment)
{
    return runCommand(workingDirectory, QStringLiteral("git"), arguments, operation, extraEnvironment);
}

GitResult<QByteArray> GitClient::runCommand(const QString &workingDirectory, const QString &program,
                                            const QStringList &arguments, const QString &operation,
                                            const QHash<QString, QString> &extraEnvironment)
{
    QProcess process;
    process.setWorkingDirectory(workingDirectory);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    environment.insert(QStringLiteral("GIT_PAGER"), QStringLiteral("cat"));
    environment.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    environment.insert(QStringLiteral("GIT_EDITOR"), QStringLiteral("true"));
    environment.insert(QStringLiteral("GIT_SEQUENCE_EDITOR"), QStringLiteral("true"));
    for (auto it = extraEnvironment.constBegin(); it != extraEnvironment.constEnd(); ++it)
        environment.insert(it.key(), it.value());
    process.setProcessEnvironment(environment);
    process.setProgram(program);
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();

    if (!process.waitForStarted(5000)) {
        // A program that cannot be started is not a command that failed: the
        // caller has to be able to tell them apart, so carry QProcess's own
        // FailedToStart code instead of the generic exit code.
        return std::unexpected(GitError{ operation, process.errorString(), QProcess::FailedToStart });
    }
    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished();
        return std::unexpected(GitError{ operation, QStringLiteral("Git command timed out") });
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return std::unexpected(processError(operation, process));
    return process.readAllStandardOutput();
}

} // namespace GitNaga
