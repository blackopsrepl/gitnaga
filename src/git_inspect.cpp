#include "git_client.hpp"

#include "git_parse.hpp"

namespace GitNaga {

using detail::decode;

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
    auto changes = run(worktree, changeArguments, QStringLiteral("list changed files"));
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

GitResult<QVector<DiffLine>> GitClient::loadDiff(const QString &worktree, const QString &oid, const QString &path,
                                                 const QString &oldPath)
{
    // A rename only diffs as a rename when both sides are named.
    QStringList arguments{ QStringLiteral("show"), QStringLiteral("--format="), QStringLiteral("--no-color"),
                           QStringLiteral("--no-ext-diff"), QStringLiteral("--first-parent"), oid,
                           QStringLiteral("--") };
    if (!oldPath.isEmpty())
        arguments.append(oldPath);
    arguments.append(path);
    auto diff = run(worktree, arguments, QStringLiteral("load diff"));
    if (!diff)
        return std::unexpected(diff.error());
    return detail::parseDiff(*diff);
}

} // namespace GitNaga
