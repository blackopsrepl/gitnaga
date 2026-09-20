#pragma once

#include "domain.hpp"

#include <QAbstractListModel>

namespace GitNaga {

class CommitModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        OidRole = Qt::UserRole + 1,
        ShortOidRole,
        ParentsRole,
        AuthorRole,
        AuthoredAtRole,
        RelativeDateRole,
        SubjectRole,
        RefsRole,
        LaneRole,
        LaneCountRole,
    };
    Q_ENUM(Role)

    explicit CommitModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void replace(QVector<Commit> commits);
    const QVector<Commit> &commits() const;
    const Commit *commitAt(int row) const;

signals:
    void countChanged();

private:
    QVector<Commit> m_commits;
};

} // namespace GitNaga
