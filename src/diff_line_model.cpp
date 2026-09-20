#include "diff_line_model.hpp"

namespace GitNaga {

DiffLineModel::DiffLineModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int DiffLineModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_lines.size());
}

QVariant DiffLineModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_lines.size())
        return {};
    const auto &line = m_lines.at(index.row());
    switch (role) {
    case KindRole: return static_cast<int>(line.kind);
    case TextRole: return line.text;
    case OldLineRole: return line.oldLine > 0 ? QVariant(line.oldLine) : QVariant{};
    case NewLineRole: return line.newLine > 0 ? QVariant(line.newLine) : QVariant{};
    default: return {};
    }
}

QHash<int, QByteArray> DiffLineModel::roleNames() const
{
    return { { KindRole, "kind" }, { TextRole, "text" }, { OldLineRole, "oldLine" }, { NewLineRole, "newLine" } };
}

void DiffLineModel::replace(QVector<DiffLine> lines)
{
    beginResetModel();
    m_lines = std::move(lines);
    endResetModel();
}

void DiffLineModel::clear()
{
    replace({});
}

} // namespace GitNaga
