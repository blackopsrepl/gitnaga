#pragma once

#include "git_navigation.hpp"

#include <QAbstractListModel>

namespace GitNaga {

// A file's lines with the commit that last changed each one. Consecutive
// lines of one commit form a group: only the group's first line carries the
// gutter text, and groups alternate shade so their edges read at a glance.
class BlameModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString path READ path NOTIFY loaded)
    Q_PROPERTY(QString revision READ revision NOTIFY loaded)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ error NOTIFY loaded)
    Q_PROPERTY(int count READ count NOTIFY loaded)
    Q_PROPERTY(int commitCount READ commitCount NOTIFY loaded)

public:
    enum Role {
        LineRole = Qt::UserRole + 1,
        TextRole,
        OidRole,
        ShortOidRole,
        AuthorRole,
        AgeRole,
        SummaryRole,
        GroupStartRole,
        GroupIndexRole,
        UncommittedRole,
    };

    explicit BlameModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString path() const { return m_path; }
    QString revision() const { return m_revision; }
    bool loading() const { return m_loading; }
    QString error() const { return m_error; }
    int count() const { return static_cast<int>(m_lines.size()); }
    int commitCount() const { return m_commitCount; }

    // Blames `path` at `revision`, or the worktree's copy when it is empty.
    void load(const QString &worktree, const QString &revision, const QString &path);
    void apply(QVector<BlameLine> lines);

signals:
    void loaded();
    void loadingChanged();

private:
    QString m_path;
    QString m_revision;
    QString m_error;
    QVector<BlameLine> m_lines;
    QVector<int> m_groups;
    QDateTime m_now;
    CancelToken m_cancel;
    quint64 m_generation = 0;
    int m_commitCount = 0;
    bool m_loading = false;
};

} // namespace GitNaga
