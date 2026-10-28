#include "file_diff.hpp"

#include <QFileInfo>

#include <algorithm>

namespace GitNaga::detail {

namespace {

QStringList sideContent(const DiffDocument &document, DiffLine::Kind kind)
{
    QStringList lines;
    for (const auto &row : document.rows) {
        if (row.kind == kind || (row.kind == DiffLine::Kind::Context && row.oldLine > 0))
            lines.append(row.content);
    }
    return lines;
}

// "Subproject commit <sha>[-dirty]" from one side of a gitlink diff.
QString subprojectCommit(const DiffDocument &document, DiffLine::Kind kind, bool &dirty)
{
    for (const auto &row : document.rows) {
        if (row.kind != kind || !row.content.startsWith(QStringLiteral("Subproject commit ")))
            continue;
        auto oid = row.content.mid(18).trimmed();
        if (oid.endsWith(QStringLiteral("-dirty"))) {
            dirty = true;
            oid.chop(6);
        }
        return oid;
    }
    return {};
}

} // namespace

bool isImagePath(const QString &path)
{
    static const QStringList suffixes = { QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                                          QStringLiteral("gif"), QStringLiteral("webp"), QStringLiteral("svg"),
                                          QStringLiteral("bmp"), QStringLiteral("ico") };
    return suffixes.contains(QFileInfo(path).suffix().toLower());
}

std::optional<LfsPointer> parseLfsPointer(const QStringList &lines)
{
    if (lines.isEmpty() || !lines.first().startsWith(QStringLiteral("version https://git-lfs.github.com/spec/")))
        return std::nullopt;
    LfsPointer pointer;
    for (const auto &line : lines) {
        if (line.startsWith(QStringLiteral("oid ")))
            pointer.oid = line.mid(4).trimmed();
        else if (line.startsWith(QStringLiteral("size ")))
            pointer.size = line.mid(5).trimmed().toLongLong();
    }
    if (pointer.oid.isEmpty())
        return std::nullopt;
    return pointer;
}

SpecialFile classifySpecial(const DiffDocument &document)
{
    SpecialFile special;
    const auto &meta = document.meta;
    if (meta.oldMode == QStringLiteral("160000") || meta.newMode == QStringLiteral("160000")) {
        special.kind = SpecialFile::Kind::Submodule;
        special.fromCommit = subprojectCommit(document, DiffLine::Kind::Deletion, special.dirty);
        special.toCommit = subprojectCommit(document, DiffLine::Kind::Addition, special.dirty);
        return special;
    }
    if (isImagePath(meta.path)) {
        special.kind = SpecialFile::Kind::Image;
        return special;
    }
    if (meta.binary) {
        special.kind = SpecialFile::Kind::Binary;
        return special;
    }
    special.lfsOld = parseLfsPointer(sideContent(document, DiffLine::Kind::Deletion));
    special.lfsNew = parseLfsPointer(sideContent(document, DiffLine::Kind::Addition));
    if (special.lfsOld || special.lfsNew) {
        special.kind = SpecialFile::Kind::Lfs;
        return special;
    }
    const bool hasLines = std::any_of(document.rows.begin(), document.rows.end(), [](const DiffRow &row) {
        return row.kind == DiffLine::Kind::Addition || row.kind == DiffLine::Kind::Deletion
               || row.kind == DiffLine::Kind::Context;
    });
    if (!hasLines)
        special.kind = SpecialFile::Kind::Empty;
    return special;
}

BlobSource oldBlobFor(const DiffRequest &request, const DiffFileMeta &meta)
{
    if (meta.added || request.untracked)
        return {};
    const auto path = request.oldPath.isEmpty() ? (meta.oldPath.isEmpty() ? request.path : meta.oldPath)
                                                : request.oldPath;
    switch (request.side) {
    case DiffSide::Commit: return { BlobSource::Kind::Object, request.oid + QStringLiteral("^:") + path };
    case DiffSide::Worktree:
    case DiffSide::Staged:
        if (request.oid.isEmpty())
            return {};
        return { BlobSource::Kind::Object, request.oid + QLatin1Char(':') + path };
    case DiffSide::Unstaged: return { BlobSource::Kind::Object, QLatin1Char(':') + path };
    }
    return {};
}

BlobSource newBlobFor(const DiffRequest &request, const DiffFileMeta &meta)
{
    if (meta.deleted)
        return {};
    switch (request.side) {
    case DiffSide::Commit: return { BlobSource::Kind::Object, request.oid + QLatin1Char(':') + request.path };
    case DiffSide::Staged: return { BlobSource::Kind::Object, QLatin1Char(':') + request.path };
    case DiffSide::Worktree:
    case DiffSide::Unstaged: return { BlobSource::Kind::File, request.path };
    }
    return {};
}

} // namespace GitNaga::detail
