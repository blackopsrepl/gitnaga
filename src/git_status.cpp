#include "git_client.hpp"

#include "git_parse.hpp"

#include <QDir>
#include <QFileInfo>

namespace GitNaga {

namespace detail {

namespace {

// The path after `count` space-separated fields; paths may contain spaces, so
// splitting the whole record would cut them apart.
QString fieldTail(const QByteArray &record, int count)
{
    qsizetype position = 0;
    for (int field = 0; field < count; ++field) {
        position = record.indexOf(' ', position);
        if (position < 0)
            return {};
        ++position;
    }
    return decode(record.mid(position));
}

QString statusLetter(char code)
{
    switch (code) {
    case 'A': return QStringLiteral("A");
    case 'D': return QStringLiteral("D");
    case 'R': return QStringLiteral("R");
    case 'C': return QStringLiteral("C");
    case 'T': return QStringLiteral("T");
    default: return QStringLiteral("M");
    }
}

FileChange trackedChange(const QByteArray &record, int pathField)
{
    FileChange file;
    const char staged = record.size() > 2 ? record.at(2) : '.';
    const char unstaged = record.size() > 3 ? record.at(3) : '.';
    file.staged = staged != '.';
    file.unstaged = unstaged != '.';
    file.status = statusLetter(file.staged ? staged : unstaged);
    file.submodule = record.size() > 5 && record.at(5) == 'S';
    file.path = fieldTail(record, pathField);
    return file;
}

} // namespace

WorktreeStatus parseStatus(const QByteArray &output)
{
    WorktreeStatus status;
    status.fingerprint = output;
    const auto records = output.split('\0');
    for (qsizetype index = 0; index < records.size(); ++index) {
        const auto &record = records.at(index);
        if (record.size() < 2)
            continue;
        if (record.startsWith("# branch.upstream ")) {
            status.upstream = decode(record.mid(18));
        } else if (record.startsWith("# branch.ab ")) {
            const auto counts = decode(record.mid(12)).split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (counts.size() == 2) {
                status.ahead = counts.at(0).mid(1).toInt();
                status.behind = counts.at(1).mid(1).toInt();
            }
        } else if (record.startsWith("1 ")) {
            status.files.append(trackedChange(record, 8));
        } else if (record.startsWith("2 ")) {
            auto file = trackedChange(record, 9);
            if (index + 1 < records.size())
                file.oldPath = decode(records.at(++index));
            status.files.append(std::move(file));
        } else if (record.startsWith("u ")) {
            FileChange file;
            file.status = QStringLiteral("U");
            file.conflicted = true;
            file.unstaged = true;
            file.path = fieldTail(record, 10);
            status.files.append(std::move(file));
        } else if (record.startsWith("? ")) {
            FileChange file;
            file.status = QStringLiteral("A");
            file.untracked = true;
            file.unstaged = true;
            file.path = decode(record.mid(2));
            status.files.append(std::move(file));
        }
    }
    return status;
}

} // namespace detail

namespace {

QString operationInProgress(const QString &gitDirectory)
{
    const QDir directory(gitDirectory);
    if (QFileInfo(directory.filePath(QStringLiteral("rebase-merge"))).isDir())
        return QStringLiteral("rebase");
    if (QFileInfo(directory.filePath(QStringLiteral("rebase-apply"))).isDir())
        return QFileInfo::exists(directory.filePath(QStringLiteral("rebase-apply/applying"))) ? QStringLiteral("am")
                                                                                              : QStringLiteral("rebase");
    if (QFileInfo::exists(directory.filePath(QStringLiteral("MERGE_HEAD"))))
        return QStringLiteral("merge");
    if (QFileInfo::exists(directory.filePath(QStringLiteral("CHERRY_PICK_HEAD"))))
        return QStringLiteral("cherry-pick");
    if (QFileInfo::exists(directory.filePath(QStringLiteral("REVERT_HEAD"))))
        return QStringLiteral("revert");
    if (QFileInfo::exists(directory.filePath(QStringLiteral("BISECT_LOG"))))
        return QStringLiteral("bisect");
    return {};
}

} // namespace

GitResult<WorktreeStatus> GitClient::loadStatus(const RepositoryLocation &location)
{
    if (location.isBare)
        return WorktreeStatus{};
    auto output = run(location.worktree,
                      { QStringLiteral("status"), QStringLiteral("--porcelain=v2"), QStringLiteral("-z"),
                        QStringLiteral("--branch"), QStringLiteral("--untracked-files=all") },
                      QStringLiteral("read worktree status"));
    if (!output)
        return std::unexpected(output.error());
    auto status = detail::parseStatus(*output);
    status.operation = operationInProgress(location.gitDirectory);
    status.fingerprint.append(status.operation.toUtf8());
    return status;
}

CommitInspection GitClient::inspectWorktree(const WorktreeStatus &status)
{
    CommitInspection inspection;
    inspection.details.oid = worktreeOid();
    inspection.details.subject = QStringLiteral("Uncommitted changes");
    inspection.files = status.files;
    return inspection;
}

} // namespace GitNaga
