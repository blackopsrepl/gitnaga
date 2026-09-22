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

GitResult<QString> GitClient::createWorktreeCommit(const QString &worktree, const QString &gitDirectory,
                                                  const QString &headOid, const QString &authorName,
                                                  const QString &authorEmail)
{
    // Copy the real index so staged stat data is reused, then stage the whole
    // worktree into the copy. Nothing here touches the repository's own index.
    QTemporaryDir scratch;
    if (!scratch.isValid())
        return std::unexpected(GitError{ QStringLiteral("uncommitted snapshot"), QStringLiteral("No temporary directory") });
    const auto scratchIndex = QDir(scratch.path()).filePath(QStringLiteral("index"));
    const auto realIndex = QDir(gitDirectory).filePath(QStringLiteral("index"));
    if (QFileInfo::exists(realIndex) && !QFile::copy(realIndex, scratchIndex))
        return std::unexpected(GitError{ QStringLiteral("uncommitted snapshot"), QStringLiteral("Cannot copy the index") });

    const QHash<QString, QString> indexEnvironment{
        { QStringLiteral("GIT_INDEX_FILE"), scratchIndex }
    };
    auto staged = run(worktree, { QStringLiteral("add"), QStringLiteral("--all") },
                      QStringLiteral("snapshot worktree"), indexEnvironment);
    if (!staged)
        return std::unexpected(staged.error());

    auto tree = run(worktree, { QStringLiteral("write-tree") },
                    QStringLiteral("snapshot worktree"), indexEnvironment);
    if (!tree)
        return std::unexpected(tree.error());
    const auto treeOid = decode(*tree).trimmed();

    // Tree equality is exact: stat-only differences collapse to the same tree,
    // so a clean worktree produces no snapshot at all.
    QString headTreeOid = QStringLiteral("4b825dc642cb6eb9a060e54bf8d69288fbee4904");
    if (!headOid.isEmpty()) {
        auto headTree = run(worktree, { QStringLiteral("rev-parse"), QStringLiteral("--verify"),
                                        headOid + QStringLiteral("^{tree}") },
                            QStringLiteral("read HEAD tree"));
        if (!headTree)
            return std::unexpected(headTree.error());
        headTreeOid = decode(*headTree).trimmed();
    }
    if (headTreeOid == treeOid)
        return QString();

    const QHash<QString, QString> identityEnvironment{
        { QStringLiteral("GIT_AUTHOR_NAME"), authorName },
        { QStringLiteral("GIT_AUTHOR_EMAIL"), authorEmail },
        { QStringLiteral("GIT_COMMITTER_NAME"), authorName },
        { QStringLiteral("GIT_COMMITTER_EMAIL"), authorEmail },
    };

    QStringList arguments{ QStringLiteral("commit-tree"), treeOid };
    if (!headOid.isEmpty())
        arguments << QStringLiteral("-p") << headOid;
    arguments << QStringLiteral("-m") << QStringLiteral("Work in progress");

    auto commit = run(worktree, arguments, QStringLiteral("snapshot worktree"), identityEnvironment);
    if (!commit)
        return std::unexpected(commit.error());
    return decode(*commit).trimmed();
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
                         QStringLiteral("--format=%x1e%H%x1f%P%x1f%an%x1f%ae%x1f%at%x1f%s") },
                       QStringLiteral("load history"));
    if (!history)
        return std::unexpected(history.error());

    for (const auto &record : history->split('\x1e')) {
        if (record.trimmed().isEmpty())
            continue;
        const auto parts = record.trimmed().split('\x1f');
        if (parts.size() < 6)
            continue;
        Commit commit;
        commit.oid = decode(parts.at(0));
        const auto parentText = decode(parts.at(1));
        if (!parentText.isEmpty())
            commit.parents = parentText.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        commit.author = decode(parts.at(2));
        commit.authorEmail = decode(parts.at(3)).trimmed().toLower();
        commit.authoredAt = QDateTime::fromSecsSinceEpoch(decode(parts.at(4)).toLongLong()).toLocalTime();
        commit.subject = decode(parts.at(5));
        commit.refs = refsByOid.value(commit.oid);
        snapshot.commits.append(std::move(commit));
    }
    // Layout runs on the real commits; the uncommitted snapshot (if any) is
    // prepended afterwards and inherits its parent's lane and colour, so
    // editing a file cannot recolour the line the user is working on.
    detail::assignGraphLayout(snapshot.commits);

    if (!isBare) {
        auto head = run(snapshot.worktree, { QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("HEAD") },
                        QStringLiteral("read HEAD"));
        const auto headOid = head ? decode(*head).trimmed() : QString();

        auto name = run(snapshot.worktree, { QStringLiteral("config"), QStringLiteral("--get"), QStringLiteral("user.name") },
                        QStringLiteral("read git identity"));
        auto mail = run(snapshot.worktree, { QStringLiteral("config"), QStringLiteral("--get"), QStringLiteral("user.email") },
                        QStringLiteral("read git identity"));
        const auto author = name && !decode(*name).trimmed().isEmpty()
            ? decode(*name).trimmed() : QStringLiteral("Work in progress");
        const auto authorEmail = mail && !decode(*mail).trimmed().isEmpty()
            ? decode(*mail).trimmed() : QStringLiteral("uncommitted@gitnaga.invalid");

        auto snapshotCommit = createWorktreeCommit(snapshot.worktree, snapshot.gitDirectory, headOid, author, authorEmail);
        if (!snapshotCommit) {
            // Snapshotting is a bonus, not a prerequisite: a read-only or
            // otherwise unwritable repository still browses history normally.
            // Report it rather than silently pretending the tree is clean.
            qWarning() << "gitnaga: cannot snapshot uncommitted work:"
                       << snapshotCommit.error().operation << snapshotCommit.error().message;
        } else if (!snapshotCommit->isEmpty()) {
            snapshot.workInProgressOid = *snapshotCommit;
            // The row is the real commit object, so every surface below the
            // client treats it like any other commit.
            Commit entry;
            entry.oid = snapshot.workInProgressOid;
            if (!headOid.isEmpty())
                entry.parents.append(headOid);
            entry.author = author;
            entry.authorEmail = authorEmail.toLower();
            entry.authoredAt = QDateTime::currentDateTime();
            entry.subject = QStringLiteral("Work in progress");
            if (!snapshot.commits.isEmpty()) {
                const auto &parent = snapshot.commits.first();
                entry.lane = parent.lane;
                entry.colorIndex = parent.colorIndex;
                entry.laneCount = parent.laneCount;
            }
            snapshot.commits.prepend(std::move(entry));
        }
    }
    return snapshot;
}

} // namespace GitNaga
