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
    case UntrackedRole: return file.untracked;
    case ConflictedRole: return file.conflicted;
    case SubmoduleRole: return file.submodule;
    default: return {};
    }
}

QHash<int, QByteArray> FileChangeModel::roleNames() const
{
    return {
        { StatusRole, "status" }, { PathRole, "path" }, { OldPathRole, "oldPath" },
        { FileNameRole, "fileName" }, { DirectoryRole, "directory" },
        { UntrackedRole, "untracked" }, { ConflictedRole, "conflicted" }, { SubmoduleRole, "submodule" },
    };
}

void FileChangeModel::replace(QVector<FileChange> files)
{
    const auto before = m_files.size();
    beginResetModel();
    m_files = std::move(files);
    endResetModel();
    if (before != m_files.size())
        emit countChanged();
}

int FileChangeModel::count() const
{
    return static_cast<int>(m_files.size());
}

const FileChange *FileChangeModel::fileAt(int row) const
{
    return row >= 0 && row < m_files.size() ? &m_files.at(row) : nullptr;
}

} // namespace GitNaga
