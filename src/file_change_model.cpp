#include "file_change_model.hpp"

#include <QFileInfo>

namespace GitNaga {

FileChangeModel::FileChangeModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FileChangeModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_files.size());
}

QVariant FileChangeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_files.size())
        return {};
    const auto &file = m_files.at(index.row());
    const QFileInfo info(file.path);
    switch (role) {
    case StatusRole: return file.status;
    case PathRole: return file.path;
    case OldPathRole: return file.oldPath;
    case FileNameRole: return info.fileName();
    case DirectoryRole: return info.path() == QStringLiteral(".") ? QString{} : info.path();
    case SelectedRole: return index.row() < m_selected.size() && m_selected.at(index.row());
    default: return {};
    }
}

QHash<int, QByteArray> FileChangeModel::roleNames() const
{
    return {
        { StatusRole, "status" }, { PathRole, "path" }, { OldPathRole, "oldPath" },
        { FileNameRole, "fileName" }, { DirectoryRole, "directory" },
        { SelectedRole, "selected" },
    };
}

void FileChangeModel::replace(QVector<FileChange> files)
{
    beginResetModel();
    m_files = std::move(files);
    // A fresh listing resets the commit selection to the index, which is what
    // a commit picks up unless the user ticks more files.
    m_selected.clear();
    m_selected.reserve(m_files.size());
    for (const auto &file : m_files)
        m_selected.append(file.staged);
    endResetModel();
    emit selectedChanged();
}

void FileChangeModel::setSelected(int row, bool selected)
{
    if (row < 0 || row >= m_selected.size() || m_selected.at(row) == selected)
        return;
    m_selected[row] = selected;
    const QModelIndex index = this->index(row);
    emit dataChanged(index, index, { SelectedRole });
    emit selectedChanged();
}

void FileChangeModel::setAllSelected(bool selected)
{
    for (int row = 0; row < m_selected.size(); ++row)
        if (m_selected.at(row) != selected)
            setSelected(row, selected);
}

int FileChangeModel::selectedCount() const
{
    int count = 0;
    for (int row = 0; row < m_selected.size(); ++row)
        count += m_selected.at(row) ? 1 : 0;
    return count;
}

QStringList FileChangeModel::stagedUnselectedPaths() const
{
    // Files the index already holds but the user excluded from the commit:
    // they must be unstaged, or the commit would silently include them.
    QStringList paths;
    for (int row = 0; row < m_files.size(); ++row) {
        const bool selected = row < m_selected.size() && m_selected.at(row);
        if (m_files.at(row).staged && !selected)
            paths.append(m_files.at(row).path);
    }
    return paths;
}

QStringList FileChangeModel::selectedPaths() const
{
    QStringList paths;
    for (int row = 0; row < m_files.size(); ++row)
        if (row < m_selected.size() && m_selected.at(row))
            paths.append(m_files.at(row).path);
    return paths;
}

const FileChange *FileChangeModel::fileAt(int row) const
{
    return row >= 0 && row < m_files.size() ? &m_files.at(row) : nullptr;
}

} // namespace GitNaga
