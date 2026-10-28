#pragma once

#include <QAbstractListModel>

namespace GitNaga {

class DiffLineModel;

// The selected diff side by side: each row names the unified row shown on
// the left (old) and on the right (new); a hunk header spans both. Rows are
// paired in C++ (DiffDocument::split), so QML only lays them out.
class SplitDiffModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        HunkRole = Qt::UserRole + 1,
        SpanningRole, // a hunk header or notice across both sides
        TextRole,
        LeftRowRole,
        LeftKindRole,
        LeftLineRole,
        LeftContentRole,
        LeftStyleRole,
        RightRowRole,
        RightKindRole,
        RightLineRole,
        RightContentRole,
        RightStyleRole,
    };

    explicit SplitDiffModel(DiffLineModel *rows);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Driven by DiffLineModel around its own reset.
    void beginReset() { beginResetModel(); }
    void endReset() { endResetModel(); }

private:
    DiffLineModel *m_rows;
};

} // namespace GitNaga
