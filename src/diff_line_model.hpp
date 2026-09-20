#pragma once

#include "domain.hpp"

#include <QAbstractListModel>

namespace GitNaga {

class DiffLineModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role { KindRole = Qt::UserRole + 1, TextRole, OldLineRole, NewLineRole };

    explicit DiffLineModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void replace(QVector<DiffLine> lines);
    void clear();

private:
    QVector<DiffLine> m_lines;
};

} // namespace GitNaga
