#pragma once

#include "diff_line_model.hpp"
#include "git_navigation.hpp"

#include <QAbstractListModel>

namespace GitNaga {

// The commits that touched one file, newest first, and the diff of that file
// in whichever of them is selected.
class FileHistoryModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString path READ path NOTIFY loaded)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ error NOTIFY loaded)
    Q_PROPERTY(int count READ count NOTIFY loaded)
    Q_PROPERTY(int selectedRow READ selectedRow NOTIFY selectedRowChanged)
    Q_PROPERTY(DiffLineModel *diffLines READ diffLines CONSTANT)

public:
    enum Role {
        OidRole = Qt::UserRole + 1,
        ShortOidRole,
        SubjectRole,
        AuthorRole,
        DateRole,
        StatusRole,
        PathRole,
        OldPathRole,
    };

    explicit FileHistoryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString path() const { return m_path; }
    bool loading() const { return m_loading; }
    QString error() const { return m_error; }
    int count() const { return static_cast<int>(m_revisions.size()); }
    int selectedRow() const { return m_selectedRow; }
    DiffLineModel *diffLines() { return &m_diffLines; }
    const QVector<FileRevision> &revisions() const { return m_revisions; }

    // Reads the history of `path` from `revision` (HEAD when empty) in
    // `worktree`, then selects the newest commit.
    void load(const QString &worktree, const QString &revision, const QString &path);
    Q_INVOKABLE void select(int row);
    Q_INVOKABLE QString oidAt(int row) const;

signals:
    void loaded();
    void loadingChanged();
    void selectedRowChanged();

private:
    QString m_worktree;
    QString m_path;
    QString m_error;
    QVector<FileRevision> m_revisions;
    DiffLineModel m_diffLines;
    CancelToken m_cancel;
    quint64 m_generation = 0;
    quint64 m_diffGeneration = 0;
    int m_selectedRow = -1;
    bool m_loading = false;
};

} // namespace GitNaga
