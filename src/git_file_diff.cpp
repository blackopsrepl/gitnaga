#include "git_client.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace GitNaga {

namespace {

qint64 blobSize(const QString &worktree, const BlobSource &source)
{
    switch (source.kind) {
    case BlobSource::Kind::None: return -1;
    case BlobSource::Kind::File: {
        const QFileInfo info(QDir(worktree).filePath(source.spec));
        return info.exists() ? info.size() : -1;
    }
    case BlobSource::Kind::Object: {
        const auto size = GitClient::run(worktree, { QStringLiteral("cat-file"), QStringLiteral("-s"), source.spec },
                                         QStringLiteral("read blob size"));
        return size ? size->trimmed().toLongLong() : -1;
    }
    }
    return -1;
}

// The commits a moved submodule pointer brings in, read from the submodule's
// own checkout; empty when it is not checked out or lacks the objects.
QStringList submoduleLog(const QString &directory, const QString &from, const QString &to)
{
    if (to.isEmpty() || !QFileInfo::exists(QDir(directory).filePath(QStringLiteral(".git"))))
        return {};
    QStringList arguments = { QStringLiteral("log"), QStringLiteral("--no-color"), QStringLiteral("--format=%h %s"),
                              QStringLiteral("-n"), QStringLiteral("50") };
    arguments.append(from.isEmpty() ? to : from + QStringLiteral("..") + to);
    const auto log = GitClient::run(directory, arguments, QStringLiteral("read submodule log"));
    if (!log)
        return {};
    return QString::fromUtf8(*log).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

} // namespace

GitResult<QByteArray> GitClient::readBlob(const QString &worktree, const BlobSource &source)
{
    switch (source.kind) {
    case BlobSource::Kind::None:
        return std::unexpected(GitError{ QStringLiteral("read blob"), QStringLiteral("No such side") });
    case BlobSource::Kind::File: {
        QFile file(QDir(worktree).filePath(source.spec));
        if (!file.open(QIODevice::ReadOnly))
            return std::unexpected(GitError{ QStringLiteral("read blob"), file.errorString() });
        return file.readAll();
    }
    case BlobSource::Kind::Object:
        // cat-file, not show: the bytes as stored, without textconv.
        return run(worktree, { QStringLiteral("cat-file"), QStringLiteral("blob"), source.spec },
                   QStringLiteral("read blob"));
    }
    return QByteArray{};
}

GitResult<FileDiff> GitClient::loadFileDiff(const DiffRequest &request, const DiffOptions &options)
{
    auto lines = loadDiff(request.worktree, request.oid, request.path, request.oldPath, request.side,
                          request.untracked, options);
    if (!lines)
        return std::unexpected(lines.error());
    FileDiff diff;
    diff.worktree = request.worktree;
    diff.document = buildDiffDocument(*lines, request.path);
    auto &meta = diff.document.meta;
    if (meta.oldPath.isEmpty() && !request.oldPath.isEmpty() && request.oldPath != request.path) {
        meta.oldPath = request.oldPath;
        meta.renamed = true;
    }
    meta.added = meta.added || request.untracked;
    diff.special = detail::classifySpecial(diff.document);
    auto &special = diff.special;
    using Kind = SpecialFile::Kind;
    if (special.kind == Kind::Binary || special.kind == Kind::Image) {
        special.oldBlob = detail::oldBlobFor(request, meta);
        special.newBlob = detail::newBlobFor(request, meta);
        special.oldSize = blobSize(request.worktree, special.oldBlob);
        special.newSize = blobSize(request.worktree, special.newBlob);
    } else if (special.kind == Kind::Submodule) {
        const auto directory = QDir(request.worktree).filePath(request.path);
        special.checkedOut = QFileInfo::exists(QDir(directory).filePath(QStringLiteral(".git")));
        special.commits = submoduleLog(directory, special.fromCommit, special.toCommit);
    }
    return diff;
}

} // namespace GitNaga
