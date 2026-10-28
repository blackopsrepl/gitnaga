#pragma once

#include "file_diff.hpp"
#include "split_diff_model.hpp"

#include <QAbstractListModel>
#include <QVariantMap>

namespace GitNaga {


// The selected file's diff, one row per displayed line (hunk headers,
// context, additions, deletions, notices), plus the file facts and options
// the diff header shows. `split` lays the same rows out side by side.
class DiffLineModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap file READ file NOTIFY documentChanged)
    Q_PROPERTY(QVariantMap special READ special NOTIFY documentChanged)
    Q_PROPERTY(int maxColumns READ maxColumns NOTIFY documentChanged)
    Q_PROPERTY(int maxLineNumber READ maxLineNumber NOTIFY documentChanged)
    Q_PROPERTY(int hunkCount READ hunkCount NOTIFY documentChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY documentChanged)
    Q_PROPERTY(bool ignoreWhitespace READ ignoreWhitespace WRITE setIgnoreWhitespace NOTIFY optionsChanged)
    Q_PROPERTY(int contextLines READ contextLines WRITE setContextLines NOTIFY optionsChanged)
    Q_PROPERTY(GitNaga::SplitDiffModel *split READ split CONSTANT)

public:
    // oldLine/newLine are null where a side has no line; hunk is the index
    // of the row's hunk (-1 for notices); pair is the row a deletion or
    // addition replaced; content is the shown text (marker dropped, tabs
    // expanded) and style its colouring, both opaque to QML.
    enum Role {
        KindRole = Qt::UserRole + 1,
        TextRole,
        OldLineRole,
        NewLineRole,
        HunkRole,
        ContentRole,
        StyleRole,
        PairRole,
    };

    explicit DiffLineModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void replace(FileDiff diff);
    // Rows only, without file facts: for callers that have just the patch.
    void replace(QVector<DiffLine> lines);
    void clear();

    const DiffDocument &document() const { return m_diff.document; }
    DiffOptions options() const { return m_options; }
    QVariantMap file() const;
    QVariantMap special() const;
    int maxColumns() const { return m_diff.document.maxColumns; }
    int maxLineNumber() const { return m_diff.document.maxLineNumber; }
    int hunkCount() const { return m_diff.document.hunkCount; }
    bool loaded() const { return m_loaded; }
    bool ignoreWhitespace() const { return m_options.ignoreWhitespace; }
    void setIgnoreWhitespace(bool ignore);
    int contextLines() const { return m_options.contextLines; }
    void setContextLines(int lines);
    SplitDiffModel *split() const { return m_split; }

    Q_INVOKABLE void copyPath() const;
    // One row's identity, for a caller acting on a line selection.
    Q_INVOKABLE QVariantMap rowInfo(int row) const;

signals:
    void documentChanged();
    void optionsChanged();

private:
    void reset(FileDiff diff, bool loaded);
    QString imageUrl(const BlobSource &source) const;

    FileDiff m_diff;
    DiffOptions m_options;
    SplitDiffModel *m_split = nullptr;
    quint64 m_generation = 0;
    bool m_loaded = false;
};

} // namespace GitNaga
