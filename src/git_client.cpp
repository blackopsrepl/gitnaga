#include "git_client.hpp"

#include "git_parse.hpp"

#include <QDir>
#include <QHash>
#include <QProcess>
#include <QProcessEnvironment>

namespace GitNaga {

using detail::decode;

namespace {

GitError processError(const QString &operation, QProcess &process)
{
    return GitError{ operation, decode(process.readAllStandardError()).trimmed(), process.exitCode() };
}

} // namespace

GitResult<QByteArray> GitClient::run(const QString &workingDirectory, const QStringList &arguments, const QString &operation)
{
    QProcess process;
    process.setWorkingDirectory(workingDirectory);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    environment.insert(QStringLiteral("GIT_PAGER"), QStringLiteral("cat"));
    environment.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    environment.insert(QStringLiteral("GIT_EDITOR"), QStringLiteral("true"));
    environment.insert(QStringLiteral("GIT_SEQUENCE_EDITOR"), QStringLiteral("true"));
    process.setProcessEnvironment(environment);
    process.setProgram(QStringLiteral("git"));
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();

    if (!process.waitForStarted(5000))
        return std::unexpected(GitError{ operation, process.errorString() });
    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished();
        return std::unexpected(GitError{ operation, QStringLiteral("Git command timed out") });
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return std::unexpected(processError(operation, process));
    return process.readAllStandardOutput();
}

GitResult<QString> GitClient::mutate(const QString &worktree, const QStringList &arguments, const QString &operation)
{
    auto output = run(worktree, arguments, operation);
    if (!output)
        return std::unexpected(output.error());
    return decode(*output).trimmed();
}

GitResult<RepositorySnapshot> GitClient::loadRepository(const QString &path, int maximumCommits)
{
    auto discovery = run(path,
                         { QStringLiteral("rev-parse"), QStringLiteral("--path-format=absolute"),
                           QStringLiteral("--git-dir"), QStringLiteral("--git-common-dir"),
                           QStringLiteral("--is-bare-repository") },
                         QStringLiteral("discover repository"));
    if (!discovery)
        return std::unexpected(discovery.error());

    const auto fields = discovery->split('\n');
    if (fields.size() < 3)
        return std::unexpected(GitError{ QStringLiteral("discover repository"), QStringLiteral("Unexpected rev-parse output") });

    RepositorySnapshot snapshot;
    snapshot.gitDirectory = QDir::cleanPath(decode(fields.at(0)));
    snapshot.commonDirectory = QDir::cleanPath(decode(fields.at(1)));
    const bool isBare = decode(fields.at(2)).trimmed() == QStringLiteral("true");
    if (isBare) {
        snapshot.worktree = QDir(path).absolutePath();
    } else {
        auto toplevel = run(path,
                            { QStringLiteral("rev-parse"), QStringLiteral("--path-format=absolute"),
                              QStringLiteral("--show-toplevel") },
                            QStringLiteral("discover worktree"));
        if (!toplevel)
            return std::unexpected(toplevel.error());
        snapshot.worktree = QDir::cleanPath(decode(*toplevel).trimmed());
    }

    auto branch = run(snapshot.worktree,
                      { QStringLiteral("symbolic-ref"), QStringLiteral("--quiet"), QStringLiteral("--short"), QStringLiteral("HEAD") },
                      QStringLiteral("read HEAD"));
    if (branch)
        snapshot.currentBranch = decode(*branch).trimmed();

    QHash<QString, QStringList> refsByOid;
    auto refsResult = run(snapshot.worktree,
                          { QStringLiteral("for-each-ref"),
                            QStringLiteral("--format=%(objectname)%00%(refname)%00") },
                          QStringLiteral("read refs"));
    if (!refsResult)
        return std::unexpected(refsResult.error());

    const auto refFields = refsResult->split('\0');
    for (qsizetype index = 0; index + 1 < refFields.size(); index += 2) {
        const auto oid = decode(refFields.at(index)).trimmed();
        auto ref = decode(refFields.at(index + 1)).trimmed();
        if (oid.isEmpty() || ref.isEmpty())
            continue;
        ref.remove(QStringLiteral("refs/heads/"));
        ref.replace(QStringLiteral("refs/remotes/"), QStringLiteral("⇄ "));
        ref.replace(QStringLiteral("refs/tags/"), QStringLiteral("# "));
        refsByOid[oid].append(ref);
    }

    auto history = run(snapshot.worktree,
                       { QStringLiteral("log"), QStringLiteral("--all"), QStringLiteral("--topo-order"),
                         QStringLiteral("--date-order"), QStringLiteral("--no-color"),
                         QStringLiteral("--max-count=%1").arg(maximumCommits),
                         QStringLiteral("--format=%x1e%H%x1f%P%x1f%an%x1f%at%x1f%s") },
                       QStringLiteral("load history"));
    if (!history)
        return std::unexpected(history.error());

    for (const auto &record : history->split('\x1e')) {
        if (record.trimmed().isEmpty())
            continue;
        const auto parts = record.trimmed().split('\x1f');
        if (parts.size() < 5)
            continue;
        Commit commit;
        commit.oid = decode(parts.at(0));
        const auto parentText = decode(parts.at(1));
        if (!parentText.isEmpty())
            commit.parents = parentText.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        commit.author = decode(parts.at(2));
        commit.authoredAt = QDateTime::fromSecsSinceEpoch(decode(parts.at(3)).toLongLong()).toLocalTime();
        commit.subject = decode(parts.at(4));
        commit.refs = refsByOid.value(commit.oid);
        snapshot.commits.append(std::move(commit));
    }
    detail::assignGraphLayout(snapshot.commits);
    return snapshot;
}

} // namespace GitNaga
