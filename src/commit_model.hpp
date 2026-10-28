#pragma once

#include "domain.hpp"

#include <QAbstractListModel>
#include <QHash>
#include <QVariantMap>

namespace GitNaga {

// The graph's rows: the history as it streams in, with the uncommitted-work
// row on top while the worktree differs from HEAD. Rows are appended chunk by
// chunk, never rebuilt, so a long history stays responsive while it loads.
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
        ColorRole,
    };
    Q_ENUM(Role)

    explicit CommitModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int count() const;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void replace(QVector<Commit> commits);
    void append(QVector<Commit> &&commits);
    // Shows or hides the uncommitted-work row; it rides HEAD's line.
    void setWorkInProgress(bool present, const QString &headOid);
    bool hasWorkInProgress() const;

    const QVector<Commit> &commits() const;
    const Commit *commitAt(int row) const;
    int rowOf(const QString &oid) const;
    const QHash<QString, int> &rowIndex() const;
    Q_INVOKABLE QVariantMap at(int row) const;
    Q_INVOKABLE int find(const QString &oid) const { return rowOf(oid); }

    static QVariantList refsToVariant(const QVector<Ref> &refs);

signals:
    void countChanged();

private:
    void reindex(qsizetype from);

    QVector<Commit> m_commits;
    QHash<QString, int> m_rowByOid;
    bool m_workInProgress = false;
};

} // namespace GitNaga
