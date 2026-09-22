#pragma once

#include "domain.hpp"

#include <QAbstractListModel>

namespace GitNaga {

class FileChangeModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectedChanged)

public:
    enum Role { StatusRole = Qt::UserRole + 1, PathRole, OldPathRole, FileNameRole, DirectoryRole, SelectedRole };

    explicit FileChangeModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void replace(QVector<FileChange> files);
    const FileChange *fileAt(int row) const;

    // Commit selection for the uncommitted-work row. Defaults mirror the
    // index, so a commit picks up exactly what the user had staged.
    Q_INVOKABLE void setSelected(int row, bool selected);
    Q_INVOKABLE void setAllSelected(bool selected);
    Q_INVOKABLE QStringList selectedPaths() const;
    Q_INVOKABLE QStringList stagedUnselectedPaths() const;
    int selectedCount() const;

signals:
    void selectedChanged();

private:
    QVector<FileChange> m_files;
    QVector<bool> m_selected;
};

} // namespace GitNaga
