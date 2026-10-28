#include "diff_line_model.hpp"


#include <QClipboard>
#include <QGuiApplication>

namespace GitNaga {

namespace {

QString specialName(SpecialFile::Kind kind)
{
    switch (kind) {
    case SpecialFile::Kind::None: return {};
    case SpecialFile::Kind::Binary: return QStringLiteral("binary");
    case SpecialFile::Kind::Image: return QStringLiteral("image");
    case SpecialFile::Kind::Submodule: return QStringLiteral("submodule");
    case SpecialFile::Kind::Lfs: return QStringLiteral("lfs");
    case SpecialFile::Kind::Empty: return QStringLiteral("empty");
    }
    return {};
}

QVariant lineNumber(int line)
{
    return line > 0 ? QVariant(line) : QVariant{};
}

} // namespace

DiffLineModel::DiffLineModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_split(new SplitDiffModel(this))
{
}

int DiffLineModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_diff.document.rows.size());
}

QVariant DiffLineModel::data(const QModelIndex &index, int role) const
{
    const auto &rows = m_diff.document.rows;
    if (!index.isValid() || index.row() < 0 || index.row() >= rows.size())
        return {};
    const auto &row = rows.at(index.row());
    switch (role) {
    case KindRole: return static_cast<int>(row.kind);
    case TextRole: return row.text;
    case OldLineRole: return lineNumber(row.oldLine);
    case NewLineRole: return lineNumber(row.newLine);
    case HunkRole: return row.hunk;
    case ContentRole: return row.content;
    case StyleRole: return QVariant::fromValue(row.style);
    case PairRole: return row.pair;
    default: return {};
    }
}

QHash<int, QByteArray> DiffLineModel::roleNames() const
{
    return { { KindRole, "kind" },       { TextRole, "text" },       { OldLineRole, "oldLine" },
             { NewLineRole, "newLine" }, { HunkRole, "hunk" },       { ContentRole, "content" },
             { StyleRole, "lineStyle" }, { PairRole, "pair" } };
}

void DiffLineModel::replace(FileDiff diff)
{
    reset(std::move(diff), true);
}

void DiffLineModel::reset(FileDiff diff, bool loaded)
{
    beginResetModel();
    m_split->beginReset();
    m_diff = std::move(diff);
    ++m_generation;
    m_loaded = loaded;
    m_split->endReset();
    endResetModel();
    emit documentChanged();
}

void DiffLineModel::replace(QVector<DiffLine> lines)
{
    FileDiff diff;
    diff.document = buildDiffDocument(lines, {});
    replace(std::move(diff));
}

void DiffLineModel::clear()
{
    reset({}, false);
}

QVariantMap DiffLineModel::file() const
{
    const auto &meta = m_diff.document.meta;
    return {
        { QStringLiteral("path"), meta.path },          { QStringLiteral("oldPath"), meta.oldPath },
        { QStringLiteral("renamed"), meta.renamed },    { QStringLiteral("copied"), meta.copied },
        { QStringLiteral("similarity"), meta.similarity }, { QStringLiteral("oldMode"), meta.oldMode },
        { QStringLiteral("newMode"), meta.newMode },    { QStringLiteral("added"), meta.added },
        { QStringLiteral("deleted"), meta.deleted },    { QStringLiteral("binary"), meta.binary },
        { QStringLiteral("additions"), meta.additions }, { QStringLiteral("deletions"), meta.deletions },
    };
}

QVariantMap DiffLineModel::special() const
{
    const auto &special = m_diff.special;
    QVariantMap map = {
        { QStringLiteral("kind"), specialName(special.kind) },
        { QStringLiteral("oldSize"), special.oldSize },
        { QStringLiteral("newSize"), special.newSize },
        { QStringLiteral("oldImage"), imageUrl(special.oldBlob) },
        { QStringLiteral("newImage"), imageUrl(special.newBlob) },
        { QStringLiteral("from"), special.fromCommit },
        { QStringLiteral("to"), special.toCommit },
        { QStringLiteral("dirty"), special.dirty },
        { QStringLiteral("checkedOut"), special.checkedOut },
        { QStringLiteral("commits"), special.commits },
    };
    if (special.lfsOld) {
        map.insert(QStringLiteral("lfsOldOid"), special.lfsOld->oid);
        map.insert(QStringLiteral("lfsOldSize"), special.lfsOld->size);
    }
    if (special.lfsNew) {
        map.insert(QStringLiteral("lfsNewOid"), special.lfsNew->oid);
        map.insert(QStringLiteral("lfsNewSize"), special.lfsNew->size);
    }
    return map;
}

// image://diffblob/<base64url(worktree \n kind \n spec)>/<generation>: the
// provider reads the blob from there; the generation makes a reloaded
// worktree file a new URL, so the image cache never shows a stale one.
QString DiffLineModel::imageUrl(const BlobSource &source) const
{
    if (source.kind == BlobSource::Kind::None)
        return {};
    const auto kind = source.kind == BlobSource::Kind::File ? QStringLiteral("file") : QStringLiteral("object");
    const auto key = (m_diff.worktree + QLatin1Char('\n') + kind + QLatin1Char('\n') + source.spec).toUtf8();
    return QStringLiteral("image://diffblob/%1/%2")
        .arg(QString::fromLatin1(key.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)))
        .arg(m_generation);
}

void DiffLineModel::setIgnoreWhitespace(bool ignore)
{
    if (ignore == m_options.ignoreWhitespace)
        return;
    m_options.ignoreWhitespace = ignore;
    emit optionsChanged();
}

void DiffLineModel::setContextLines(int lines)
{
    if (lines == m_options.contextLines)
        return;
    m_options.contextLines = lines;
    emit optionsChanged();
}

void DiffLineModel::copyPath() const
{
    if (auto *clipboard = QGuiApplication::clipboard(); clipboard && !m_diff.document.meta.path.isEmpty())
        clipboard->setText(m_diff.document.meta.path);
}

QVariantMap DiffLineModel::rowInfo(int row) const
{
    const auto &rows = m_diff.document.rows;
    if (row < 0 || row >= rows.size())
        return {};
    const auto &line = rows.at(row);
    return { { QStringLiteral("row"), row },
             { QStringLiteral("kind"), static_cast<int>(line.kind) },
             { QStringLiteral("hunk"), line.hunk },
             { QStringLiteral("oldLine"), line.oldLine },
             { QStringLiteral("newLine"), line.newLine },
             { QStringLiteral("text"), line.text } };
}

} // namespace GitNaga
