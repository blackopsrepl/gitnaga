#include "file_history_model.hpp"

#include <QFutureWatcher>
#include <QLocale>
#include <QtConcurrentRun>

namespace GitNaga {

FileHistoryModel::FileHistoryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FileHistoryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant FileHistoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_revisions.size())
        return {};
    const auto &revision = m_revisions.at(index.row());
    switch (role) {
    case OidRole: return revision.oid;
    case ShortOidRole: return revision.oid.left(8);
    case SubjectRole: return revision.subject;
    case AuthorRole: return revision.author;
    case DateRole: return QLocale().toString(revision.authoredAt.toLocalTime(), QLocale::ShortFormat);
    case StatusRole: return revision.status;
    case PathRole: return revision.path;
    case OldPathRole: return revision.oldPath;
    default: return {};
    }
}

QHash<int, QByteArray> FileHistoryModel::roleNames() const
{
    return {
        { OidRole, "oid" },        { ShortOidRole, "shortOid" }, { SubjectRole, "subject" },
        { AuthorRole, "author" },  { DateRole, "date" },         { StatusRole, "status" },
        { PathRole, "path" },      { OldPathRole, "oldPath" },
    };
}

QString FileHistoryModel::oidAt(int row) const
{
    return row >= 0 && row < m_revisions.size() ? m_revisions.at(row).oid : QString();
}

void FileHistoryModel::load(const QString &worktree, const QString &revision, const QString &path)
{
    if (m_cancel)
        m_cancel->store(true);
    m_cancel = std::make_shared<std::atomic_bool>(false);
    const auto generation = ++m_generation;
    m_worktree = worktree;
    beginResetModel();
    m_path = path;
    m_error.clear();
    m_revisions.clear();
    endResetModel();
    m_selectedRow = -1;
    m_diffLines.clear();
    m_loading = true;
    emit loadingChanged();
    emit loaded();
    emit selectedRowChanged();

    auto *watcher = new QFutureWatcher<GitResult<QVector<FileRevision>>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation] {
        watcher->deleteLater();
        if (generation != m_generation)
            return;
        const auto result = watcher->result();
        beginResetModel();
        m_revisions = result ? *result : QVector<FileRevision>{};
        m_error = result ? QString() : result.error().message;
        endResetModel();
        m_loading = false;
        emit loadingChanged();
        emit loaded();
        select(m_revisions.isEmpty() ? -1 : 0);
    });
    watcher->setFuture(QtConcurrent::run([worktree, revision, path, cancel = m_cancel] {
        return fileHistory(worktree, revision, path, cancel);
    }));
}

void FileHistoryModel::select(int row)
{
    if (row < -1 || row >= m_revisions.size())
        return;
    m_selectedRow = row;
    emit selectedRowChanged();
    const auto generation = ++m_diffGeneration;
    m_diffLines.clear();
    if (row < 0)
        return;
    // The diff names the path the file had in that commit, and the old path
    // too when it was renamed there, so a rename reads as one.
    const auto revision = m_revisions.at(row);
    auto *watcher = new QFutureWatcher<GitResult<QVector<DiffLine>>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation] {
        watcher->deleteLater();
        if (generation != m_diffGeneration)
            return;
        if (const auto result = watcher->result())
            m_diffLines.replace(*result);
    });
    watcher->setFuture(QtConcurrent::run([worktree = m_worktree, revision] {
        return GitClient::loadDiff(worktree, revision.oid, revision.path, revision.oldPath, DiffSide::Commit);
    }));
}

} // namespace GitNaga
