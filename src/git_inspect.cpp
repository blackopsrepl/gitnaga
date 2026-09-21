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

GitResult<QVector<DiffLine>> GitClient::loadDiff(const QString &worktree, const QString &oid, const QString &path)
{
    auto diff = run(worktree,
                    { QStringLiteral("show"), QStringLiteral("--format="), QStringLiteral("--no-color"),
                      QStringLiteral("--no-ext-diff"), QStringLiteral("--first-parent"), oid,
                      QStringLiteral("--"), path },
                    QStringLiteral("load diff"));
    if (!diff)
        return std::unexpected(diff.error());
    return detail::parseDiff(*diff);
}

GitResult<CommitInspection> GitClient::inspectWorktree(const QString &worktree)
{
    auto state = readStatus(worktree);
    if (!state)
        return std::unexpected(state.error());

    CommitInspection inspection;
    inspection.details.subject = QStringLiteral("Work in progress");

    auto output = run(worktree,
                      { QStringLiteral("status"), QStringLiteral("--porcelain=v1"), QStringLiteral("-z"),
                        QStringLiteral("--untracked-files=all") },
                      QStringLiteral("list worktree changes"));
    if (!output)
        return std::unexpected(output.error());

    const auto fields = output->split('\0');
    for (qsizetype index = 0; index < fields.size(); ++index) {
        const auto entry = decode(fields.at(index));
        if (entry.size() < 3)
            continue;
        const QChar stagedCode = entry.at(0);
        const QChar unstagedCode = entry.at(1);
        FileChange file;
        if (stagedCode == QLatin1Char('?') && unstagedCode == QLatin1Char('?')) {
            file.status = QStringLiteral("A");
            file.path = entry.mid(3);
            inspection.files.append(std::move(file));
            continue;
        }
        file.path = entry.mid(3);
        if (stagedCode == QLatin1Char('R') || stagedCode == QLatin1Char('C')) {
            file.status = stagedCode == QLatin1Char('R') ? QStringLiteral("R") : QStringLiteral("C");
            if (index + 1 < fields.size())
                file.oldPath = decode(fields.at(++index));
        } else if (stagedCode == QLatin1Char('A') || stagedCode == QLatin1Char('D')) {
            file.status = stagedCode;
        } else if (unstagedCode == QLatin1Char('D')) {
            file.status = QStringLiteral("D");
        } else {
            file.status = QStringLiteral("M");
        }
        inspection.files.append(std::move(file));
    }
    return inspection;
}

GitResult<QVector<DiffLine>> GitClient::loadWorktreeDiff(const QString &worktree, const QString &path, const QString &oldPath)
{
    // Working tree versus HEAD covers staged and unstaged edits in one view.
    // A staged rename only diffs correctly when both paths are named.
    QStringList arguments = { QStringLiteral("diff"), QStringLiteral("--no-color"),
                              QStringLiteral("--no-ext-diff"), QStringLiteral("HEAD"), QStringLiteral("--") };
    if (!oldPath.isEmpty())
        arguments.append(oldPath);
    arguments.append(path);
    auto tracked = run(worktree, arguments, QStringLiteral("load worktree diff"));
    if (!tracked)
        return std::unexpected(tracked.error());
    if (!tracked->isEmpty())
        return detail::parseDiff(*tracked);

    // Untracked files are invisible to `git diff`, so fall back to the
    // no-index form against the empty device; git exits 1 to signal that a
    // difference exists, which runAllowingDiffExit accepts.
    auto untracked = runAllowingDiffExit(worktree,
                                         { QStringLiteral("diff"), QStringLiteral("--no-color"),
                                           QStringLiteral("--no-ext-diff"), QStringLiteral("--no-index"),
                                           QStringLiteral("--"), QStringLiteral("/dev/null"), path },
                                         QStringLiteral("load untracked diff"));
    if (!untracked)
        return std::unexpected(untracked.error());
    return detail::parseDiff(*untracked);
}

} // namespace GitNaga
