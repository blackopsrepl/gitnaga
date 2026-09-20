#include "git_client.hpp"

#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>

#include <algorithm>

namespace GitNaga {
namespace {

QString decode(const QByteArray &value)
{
    return QString::fromUtf8(value);
}

void assignGraphLayout(QVector<Commit> &commits)
{
    QStringList lanes;
    for (auto &commit : commits) {
        int lane = static_cast<int>(lanes.indexOf(commit.oid));
        if (lane < 0) {
            lane = static_cast<int>(lanes.size());
            lanes.append(commit.oid);
        }

        const auto before = lanes;
        lanes.removeAt(lane);

        int insertion = lane;
        for (const auto &parent : commit.parents) {
            if (!lanes.contains(parent)) {
                lanes.insert(std::min(insertion, static_cast<int>(lanes.size())), parent);
                ++insertion;
            }
        }

        commit.lane = lane;
        commit.laneCount = static_cast<int>(std::max(before.size(), lanes.size()));

        for (int from = 0; from < before.size(); ++from) {
            const auto &oid = before.at(from);
            if (oid == commit.oid)
                continue;
            const int to = static_cast<int>(lanes.indexOf(oid));
            if (to >= 0)
                commit.segments.append({ from, to });
        }
        for (const auto &parent : commit.parents) {
            const int to = static_cast<int>(lanes.indexOf(parent));
            if (to >= 0)
                commit.segments.append({ lane, to });
        }
    }
}

QVector<DiffLine> parseDiff(const QByteArray &data)
{
    QVector<DiffLine> result;
    int oldLine = 0;
    int newLine = 0;
    const QRegularExpression hunkExpression(QStringLiteral(R"(^@@ -(\d+)(?:,\d+)? \+(\d+)(?:,\d+)? @@)"));

    for (const auto &rawLine : data.split('\n')) {
        const auto text = decode(rawLine);
        DiffLine line;
        line.text = text;

        if (text.startsWith(QStringLiteral("@@"))) {
            line.kind = DiffLine::Kind::Hunk;
            const auto match = hunkExpression.match(text);
            if (match.hasMatch()) {
                oldLine = match.captured(1).toInt();
                newLine = match.captured(2).toInt();
            }
        } else if (text.startsWith(QStringLiteral("diff --git"))
                   || text.startsWith(QStringLiteral("index "))
                   || text.startsWith(QStringLiteral("--- "))
                   || text.startsWith(QStringLiteral("+++ "))) {
            line.kind = DiffLine::Kind::Header;
        } else if (text.startsWith(QLatin1Char('+'))) {
            line.kind = DiffLine::Kind::Addition;
            line.newLine = newLine++;
        } else if (text.startsWith(QLatin1Char('-'))) {
            line.kind = DiffLine::Kind::Deletion;
            line.oldLine = oldLine++;
        } else {
            line.kind = DiffLine::Kind::Context;
            if (!text.startsWith(QLatin1Char('\\'))) {
                line.oldLine = oldLine++;
                line.newLine = newLine++;
            }
        }
        result.append(std::move(line));
    }
    return result;
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
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return std::unexpected(GitError{
            operation,
            decode(process.readAllStandardError()).trimmed(),
            process.exitCode(),
        });
    }
    return process.readAllStandardOutput();
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

    auto refsResult = run(snapshot.worktree,
                          { QStringLiteral("for-each-ref"),
                            QStringLiteral("--format=%(objectname)%00%(refname)%00") },
                          QStringLiteral("read refs"));
    if (!refsResult)
        return std::unexpected(refsResult.error());

    QHash<QString, QStringList> refsByOid;
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
    assignGraphLayout(snapshot.commits);
    return snapshot;
}

GitResult<CommitInspection> GitClient::inspectCommit(const QString &worktree, const QString &oid)
{
    auto metadata = run(worktree,
                        { QStringLiteral("show"), QStringLiteral("-s"), QStringLiteral("--no-color"),
                          QStringLiteral("--format=%H%x00%P%x00%an%x00%ae%x00%at%x00%s%x00%b"), oid },
                        QStringLiteral("inspect commit"));
    if (!metadata)
        return std::unexpected(metadata.error());
    const auto fields = metadata->split('\0');
    if (fields.size() < 7)
        return std::unexpected(GitError{ QStringLiteral("inspect commit"), QStringLiteral("Unexpected commit metadata") });

    CommitInspection inspection;
    inspection.details.oid = decode(fields.at(0));
    const auto parents = decode(fields.at(1));
    if (!parents.isEmpty())
        inspection.details.parents = parents.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    inspection.details.author = decode(fields.at(2));
    inspection.details.authorEmail = decode(fields.at(3));
    inspection.details.authoredAt = QDateTime::fromSecsSinceEpoch(decode(fields.at(4)).toLongLong()).toLocalTime();
    inspection.details.subject = decode(fields.at(5));
    inspection.details.body = decode(fields.at(6)).trimmed();

    QStringList changeArguments;
    if (inspection.details.parents.isEmpty()) {
        changeArguments = { QStringLiteral("diff-tree"), QStringLiteral("--root"), QStringLiteral("--no-commit-id"),
                            QStringLiteral("--name-status"), QStringLiteral("-r"), QStringLiteral("-z"),
                            QStringLiteral("-M"), oid };
    } else {
        changeArguments = { QStringLiteral("diff"), QStringLiteral("--name-status"), QStringLiteral("-z"),
                            QStringLiteral("-M"), inspection.details.parents.first(), oid };
    }
    auto changes = run(worktree,
                       changeArguments,
                       QStringLiteral("list changed files"));
    if (!changes)
        return std::unexpected(changes.error());

    const auto parts = changes->split('\0');
    for (qsizetype index = 0; index < parts.size();) {
        const auto status = decode(parts.at(index++)).trimmed();
        if (status.isEmpty() || index >= parts.size())
            break;
        FileChange file;
        file.status = status.left(1);
        if ((status.startsWith(QLatin1Char('R')) || status.startsWith(QLatin1Char('C'))) && index + 1 < parts.size()) {
            file.oldPath = decode(parts.at(index++));
            file.path = decode(parts.at(index++));
        } else {
            file.path = decode(parts.at(index++));
        }
        inspection.files.append(std::move(file));
    }
    return inspection;
}

GitResult<QVector<DiffLine>> GitClient::loadDiff(const QString &worktree, const QString &oid, const QString &path)
{
    auto diff = run(worktree,
                    { QStringLiteral("show"), QStringLiteral("--format="), QStringLiteral("--no-color"),
                      QStringLiteral("--no-ext-diff"), QStringLiteral("--first-parent"), oid,
                      QStringLiteral("--"), path },
                    QStringLiteral("load diff"));
    if (!diff)
        return std::unexpected(diff.error());
    return parseDiff(*diff);
}

} // namespace GitNaga
