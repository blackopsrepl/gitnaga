#include "commit_model.hpp"

#include <QLocale>

namespace GitNaga {

CommitModel::CommitModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CommitModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_commits.size());
}

QVariant CommitModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_commits.size())
        return {};

    const auto &commit = m_commits.at(index.row());
    switch (role) {
    case OidRole: return commit.oid;
    case ShortOidRole: return commit.oid.left(8);
    case ParentsRole: return commit.parents;
    case AuthorRole: return commit.author;
    case AuthoredAtRole: return commit.authoredAt;
    case RelativeDateRole: {
        const auto seconds = commit.authoredAt.secsTo(QDateTime::currentDateTime());
        if (seconds < 60)
            return tr("just now");
        if (seconds < 3600)
            return tr("%1m ago").arg(seconds / 60);
        if (seconds < 86400)
            return tr("%1h ago").arg(seconds / 3600);
        if (seconds < 604800)
            return tr("%1d ago").arg(seconds / 86400);
        return QLocale().toString(commit.authoredAt.date(), QLocale::ShortFormat);
    }
    case SubjectRole: return commit.subject;
    case RefsRole: return commit.refs;
    case LaneRole: return commit.lane;
    case LaneCountRole: return commit.laneCount;
    default: return {};
    }
}

QHash<int, QByteArray> CommitModel::roleNames() const
{
    return {
        { OidRole, "oid" },
        { ShortOidRole, "shortOid" },
        { ParentsRole, "parents" },
        { AuthorRole, "author" },
        { AuthoredAtRole, "authoredAt" },
        { RelativeDateRole, "relativeDate" },
        { SubjectRole, "subject" },
        { RefsRole, "refs" },
        { LaneRole, "lane" },
        { LaneCountRole, "laneCount" },
    };
}

void CommitModel::replace(QVector<Commit> commits)
{
    beginResetModel();
    m_commits = std::move(commits);
    endResetModel();
}

const QVector<Commit> &CommitModel::commits() const
{
    return m_commits;
}

const Commit *CommitModel::commitAt(int row) const
{
    return row >= 0 && row < m_commits.size() ? &m_commits.at(row) : nullptr;
}

} // namespace GitNaga
