#include "git_rebase.hpp"

#include "git_parse.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryFile>

namespace GitNaga::gitrebase {

using detail::decode;

namespace {

GitResult<QString> git(const QString &worktree, const QStringList &arguments, const QString &operation,
                       const RunOptions &options = {})
{
    return GitClient::mutate(worktree, arguments, operation, options);
}

GitError failure(const QString &operation, const QString &message)
{
    return GitError{ operation, message };
}

bool stoppedRebase(const QString &gitDirectory)
{
    return QDir(gitDirectory + QStringLiteral("/rebase-merge")).exists()
        || QDir(gitDirectory + QStringLiteral("/rebase-apply")).exists();
}

bool writeText(const QString &path, const QString &text)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(text.toUtf8()) >= 0;
}

// Fields of `git log` records: oid, parents, subject, full message.
QVector<QStringList> logRecords(const QByteArray &output)
{
    QVector<QStringList> records;
    for (const auto &record : output.split('\x1e')) {
        const auto fields = decode(record).split(QLatin1Char('\x1f'));
        if (fields.size() >= 4 && !fields.at(0).trimmed().isEmpty())
            records.append({ fields.at(0).trimmed(), fields.at(1), fields.at(2), fields.at(3) });
    }
    return records;
}

GitResult<QStringList> parentsOf(const QString &worktree, const QString &oid)
{
    auto parents = git(worktree, { QStringLiteral("log"), QStringLiteral("-1"), QStringLiteral("--format=%P"), oid },
                       QStringLiteral("read commit"));
    if (!parents)
        return std::unexpected(parents.error());
    return parents->split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

} // namespace

GitResult<Range> loadRange(const QString &worktree, const QString &oid)
{
    const auto operation = QStringLiteral("interactive rebase");
    // Exit 1 (not an ancestor) and an unknown oid both mean there is nothing
    // of HEAD's to rewrite from here.
    auto contained = git(worktree, { QStringLiteral("merge-base"), QStringLiteral("--is-ancestor"), oid,
                                     QStringLiteral("HEAD") }, operation);
    if (!contained)
        return std::unexpected(failure(operation, QStringLiteral("%1 is not part of HEAD's history.").arg(oid.left(8))));
    auto parents = parentsOf(worktree, oid);
    if (!parents)
        return std::unexpected(parents.error());

    Range range;
    range.base = parents->value(0);
    auto log = GitClient::run(worktree, { QStringLiteral("log"), QStringLiteral("--reverse"),
                                          QStringLiteral("--format=%H%x1f%P%x1f%s%x1f%B%x1e"),
                                          range.base.isEmpty() ? QStringLiteral("HEAD") : range.base + QStringLiteral("..HEAD") },
                              operation);
    if (!log)
        return std::unexpected(log.error());
    for (const auto &record : logRecords(*log)) {
        if (record.at(1).split(QLatin1Char(' '), Qt::SkipEmptyParts).size() > 1) {
            return std::unexpected(failure(operation,
                QStringLiteral("The range contains the merge %1 (\"%2\"). Rewriting it as a straight line would "
                               "flatten the merge; start from a commit after it instead.")
                    .arg(record.at(0).left(8), record.at(2))));
        }
        range.steps.append({ rebase::Action::Pick, record.at(0), record.at(2), record.at(3), {} });
    }
    return range;
}

GitResult<QString> run(const QString &worktree, const QString &base, const QVector<rebase::Step> &steps, bool autostash,
                       const RunOptions &options)
{
    const auto operation = QStringLiteral("interactive rebase");
    auto gitDirectory = git(worktree, { QStringLiteral("rev-parse"), QStringLiteral("--absolute-git-dir") }, operation);
    if (!gitDirectory)
        return std::unexpected(gitDirectory.error());
    if (stoppedRebase(*gitDirectory))
        return std::unexpected(failure(operation, QStringLiteral("A rebase is already in progress; continue or abort it first.")));
    if (!autostash) {
        auto dirty = git(worktree, { QStringLiteral("status"), QStringLiteral("--porcelain"),
                                     QStringLiteral("--untracked-files=no") }, operation);
        if (!dirty)
            return std::unexpected(dirty.error());
        if (!dirty->isEmpty())
            return std::unexpected(failure(operation, QStringLiteral("The worktree has uncommitted changes. Commit or "
                                                                     "stash them, or rebase with autostash.")));
    }

    // Messages live inside the git directory so an exec line that only runs
    // after a stop and `rebase --continue` still finds its file.
    const auto messageDirectory = *gitDirectory + QStringLiteral("/gitnaga-rebase");
    QDir(messageDirectory).removeRecursively();
    auto todo = rebase::buildTodo(steps, messageDirectory);
    if (!todo)
        return std::unexpected(failure(operation, todo.error()));
    QDir().mkpath(messageDirectory);
    for (const auto &message : todo->messages) {
        if (!writeText(message.path, message.text))
            return std::unexpected(failure(operation, QStringLiteral("Cannot write %1").arg(message.path)));
    }
    // The todo is only read while git starts, by the sequence editor below, so
    // a temporary file that lives as long as this call is enough.
    QTemporaryFile todoFile(QDir::tempPath() + QStringLiteral("/gitnaga-todo-XXXXXX"));
    if (!todoFile.open() || todoFile.write(todo->text.toUtf8()) < 0 || !todoFile.flush())
        return std::unexpected(failure(operation, QStringLiteral("Cannot write the rebase todo")));

    auto runOptions = options;
    // git calls the sequence editor as `<editor> <its todo path>`, so copying
    // ours over it is all the editing there is.
    runOptions.environment.insert(QStringLiteral("GIT_SEQUENCE_EDITOR"),
                                  QStringLiteral("cp ") + rebase::shellQuote(todoFile.fileName()));
    QStringList arguments{ QStringLiteral("rebase"), QStringLiteral("--interactive") };
    if (autostash)
        arguments.append(QStringLiteral("--autostash"));
    arguments.append(base.isEmpty() ? QStringLiteral("--root") : base);
    auto result = git(worktree, arguments, operation, runOptions);
    if (!stoppedRebase(*gitDirectory))
        QDir(messageDirectory).removeRecursively();
    return result;
}

GitResult<QString> reword(const QString &worktree, const QString &oid, const QString &message, const RunOptions &options)
{
    auto range = loadRange(worktree, oid);
    if (!range)
        return std::unexpected(range.error());
    range->steps.first().action = rebase::Action::Reword;
    range->steps.first().message = message;
    return run(worktree, range->base, range->steps, false, options);
}

GitResult<QString> squashIntoParent(const QString &worktree, const QString &oid, const RunOptions &options)
{
    auto parents = parentsOf(worktree, oid);
    if (!parents)
        return std::unexpected(parents.error());
    if (parents->size() != 1)
        return std::unexpected(failure(QStringLiteral("squash"), QStringLiteral("%1 has no single parent to squash into.")
                                                                     .arg(oid.left(8))));
    auto range = loadRange(worktree, parents->first());
    if (!range)
        return std::unexpected(range.error());
    if (range->steps.size() < 2 || range->steps.at(1).oid != oid)
        return std::unexpected(failure(QStringLiteral("squash"), QStringLiteral("%1 is not part of HEAD's history.")
                                                                     .arg(oid.left(8))));
    range->steps[1].action = rebase::Action::Squash;
    return run(worktree, range->base, range->steps, false, options);
}

GitResult<QString> drop(const QString &worktree, const QString &oid, const RunOptions &options)
{
    auto range = loadRange(worktree, oid);
    if (!range)
        return std::unexpected(range.error());
    range->steps.first().action = rebase::Action::Drop;
    return run(worktree, range->base, range->steps, false, options);
}

} // namespace GitNaga::gitrebase
