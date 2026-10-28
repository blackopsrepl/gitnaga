#include "git_client.hpp"

#include "git_parse.hpp"

namespace GitNaga {

using detail::decode;

namespace {

// A generated file or a vendored blob can produce a diff of hundreds of
// megabytes; nobody reads that line by line, and the view should not stall
// trying to. Show the head and say what was left out.
constexpr qsizetype kDiffByteLimit = 4 * 1024 * 1024;

QVector<DiffLine> boundedDiff(const QByteArray &output)
{
    if (output.size() <= kDiffByteLimit)
        return detail::parseDiff(output);
    auto cut = output.lastIndexOf('\n', kDiffByteLimit);
    if (cut < 0)
        cut = kDiffByteLimit;
    auto lines = detail::parseDiff(output.left(cut));
    DiffLine notice;
    notice.kind = DiffLine::Kind::Hunk;
    notice.text = QStringLiteral("… diff truncated: %1 MB not shown")
                      .arg(static_cast<double>(output.size() - cut) / (1024.0 * 1024.0), 0, 'f', 1);
    lines.append(notice);
    return lines;
}

} // namespace

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
                                                 const QString &oldPath, DiffSide side, bool untracked)
{
    RunOptions options;
    QStringList arguments;
    if (untracked && side != DiffSide::Commit && side != DiffSide::Staged) {
        // Git has no blob for an untracked file yet; --no-index diffs it
        // against nothing and exits 1 to say "they differ".
        options.acceptedExitCodes = { 0, 1 };
        arguments = { QStringLiteral("diff"), QStringLiteral("--no-color"), QStringLiteral("--no-ext-diff"),
                      QStringLiteral("--no-index"), QStringLiteral("--"), QStringLiteral("/dev/null"), path };
    } else {
        switch (side) {
        case DiffSide::Commit:
            arguments = { QStringLiteral("show"), QStringLiteral("--format="), QStringLiteral("--no-color"),
                          QStringLiteral("--no-ext-diff"), QStringLiteral("-M"), QStringLiteral("--first-parent"), oid };
            break;
        case DiffSide::Worktree:
            // Against HEAD, or the empty tree on an unborn branch.
            arguments = { QStringLiteral("diff"), QStringLiteral("--no-color"), QStringLiteral("--no-ext-diff"),
                          QStringLiteral("-M"),
                          oid.isEmpty() ? QStringLiteral("4b825dc642cb6eb9a060e54bf8d69288fbee4904") : oid };
            break;
        case DiffSide::Unstaged:
            arguments = { QStringLiteral("diff"), QStringLiteral("--no-color"), QStringLiteral("--no-ext-diff") };
            break;
        case DiffSide::Staged:
            arguments = { QStringLiteral("diff"), QStringLiteral("--cached"), QStringLiteral("--no-color"),
                          QStringLiteral("--no-ext-diff"), QStringLiteral("-M") };
            break;
        }
        // A rename only diffs as a rename when both sides are named.
        arguments.append(QStringLiteral("--"));
        if (!oldPath.isEmpty())
            arguments.append(oldPath);
        arguments.append(path);
    }
    auto diff = run(worktree, arguments, QStringLiteral("load diff"), options);
    if (!diff)
        return std::unexpected(diff.error());
    return boundedDiff(*diff);
}

} // namespace GitNaga
