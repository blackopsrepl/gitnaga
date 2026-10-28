#pragma once

#include "domain.hpp"

#include <QAbstractListModel>

namespace GitNaga {

class FileChangeModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role { StatusRole = Qt::UserRole + 1, PathRole, OldPathRole, FileNameRole, DirectoryRole, UntrackedRole,
                ConflictedRole, SubmoduleRole };

    explicit FileChangeModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void replace(QVector<FileChange> files);
    const FileChange *fileAt(int row) const;
    const QVector<FileChange> &files() const { return m_files; }

    int count() const;

signals:
    void countChanged();

private:
    QVector<FileChange> m_files;
};

} // namespace GitNaga
