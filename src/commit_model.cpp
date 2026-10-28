#include "commit_model.hpp"

#include "git_refs.hpp"
#include "graph_palette.hpp"

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

int CommitModel::count() const
{
    return static_cast<int>(m_commits.size());
}

QVariantList CommitModel::refsToVariant(const QVector<Ref> &refs)
{
    QVariantList list;
    for (const auto &ref : refs) {
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), ref.name);
        entry.insert(QStringLiteral("fullName"), ref.fullName);
        entry.insert(QStringLiteral("kind"), gitrefs::kindName(ref.kind));
        entry.insert(QStringLiteral("remote"), ref.remote);
        entry.insert(QStringLiteral("branch"), ref.branch);
        entry.insert(QStringLiteral("isHead"), ref.isHead);
        entry.insert(QStringLiteral("upstream"), ref.upstream);
        entry.insert(QStringLiteral("ahead"), ref.ahead);
        entry.insert(QStringLiteral("behind"), ref.behind);
        list.append(entry);
    }
    return list;
}

QVariant CommitModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_commits.size())
        return {};

    const auto &commit = m_commits.at(index.row());
    switch (role) {
    case OidRole: return commit.oid;
    case ShortOidRole: return commit.oid == worktreeOid() ? QString() : commit.oid.left(8);
    case ParentsRole: return commit.parents;
    case AuthorRole: return commit.author;
    case AuthoredAtRole: return commit.authoredAt;
    case RelativeDateRole: {
        if (commit.oid == worktreeOid())
            return QString();
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
    case RefsRole: return refsToVariant(commit.refs);
    case LaneRole: return commit.lane;
    case LaneCountRole: return commit.laneCount;
    case ColorRole: return graph::laneColor(commit.colorIndex).name();
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
        { ColorRole, "color" },
    };
}

void CommitModel::reindex(qsizetype from)
{
    if (from == 0)
        m_rowByOid.clear();
    m_rowByOid.reserve(m_commits.size());
    for (qsizetype row = from; row < m_commits.size(); ++row)
        m_rowByOid.insert(m_commits.at(row).oid, static_cast<int>(row));
}

void CommitModel::replace(QVector<Commit> commits)
{
    beginResetModel();
    m_commits = std::move(commits);
    m_workInProgress = !m_commits.isEmpty() && m_commits.first().oid == worktreeOid();
    reindex(0);
    endResetModel();
    emit countChanged();
}

void CommitModel::append(QVector<Commit> &&commits)
{
    if (commits.isEmpty())
        return;
    const auto first = m_commits.size();
    beginInsertRows({}, static_cast<int>(first), static_cast<int>(first + commits.size() - 1));
    m_commits.append(std::move(commits));
    reindex(first);
    endInsertRows();
    emit countChanged();
}

void CommitModel::setWorkInProgress(bool present, const QString &headOid)
{
    if (present == m_workInProgress) {
        if (!present)
            return;
        // Already shown: only its line may have moved.
        const int head = rowOf(headOid);
        if (head > 0) {
            m_commits[0].lane = m_commits.at(head).lane;
            m_commits[0].colorIndex = m_commits.at(head).colorIndex;
            m_commits[0].laneCount = m_commits.at(head).laneCount;
            emit dataChanged(index(0), index(0));
        }
        return;
    }
    if (!present) {
        beginRemoveRows({}, 0, 0);
        m_commits.removeFirst();
        m_workInProgress = false;
        reindex(0);
        endRemoveRows();
        emit countChanged();
        return;
    }
    Commit entry;
    entry.oid = worktreeOid();
    if (!headOid.isEmpty())
        entry.parents.append(headOid);
    entry.subject = tr("Uncommitted changes");
    entry.authoredAt = QDateTime::currentDateTime();
    // The row rides HEAD's line and colour, so editing a file can never
    // recolour the branch the user is working on.
    if (const int head = rowOf(headOid); head >= 0) {
        entry.lane = m_commits.at(head).lane;
        entry.colorIndex = m_commits.at(head).colorIndex;
        entry.laneCount = m_commits.at(head).laneCount;
    }
    beginInsertRows({}, 0, 0);
    m_commits.prepend(std::move(entry));
    m_workInProgress = true;
    reindex(0);
    endInsertRows();
    emit countChanged();
}

bool CommitModel::hasWorkInProgress() const { return m_workInProgress; }

const QVector<Commit> &CommitModel::commits() const
{
    return m_commits;
}

const Commit *CommitModel::commitAt(int row) const
{
    return row >= 0 && row < m_commits.size() ? &m_commits.at(row) : nullptr;
}

int CommitModel::rowOf(const QString &oid) const
{
    return m_rowByOid.value(oid, -1);
}

const QHash<QString, int> &CommitModel::rowIndex() const
{
    return m_rowByOid;
}

QVariantMap CommitModel::at(int row) const
{
    const auto *commit = commitAt(row);
    if (!commit)
        return {};
    QVariantMap map;
    map.insert(QStringLiteral("oid"), commit->oid);
    map.insert(QStringLiteral("shortOid"), commit->oid.left(8));
    map.insert(QStringLiteral("subject"), commit->subject);
    map.insert(QStringLiteral("author"), commit->author);
    map.insert(QStringLiteral("refs"), refsToVariant(commit->refs));
    map.insert(QStringLiteral("parents"), commit->parents);
    map.insert(QStringLiteral("lane"), commit->lane);
    map.insert(QStringLiteral("color"), graph::laneColor(commit->colorIndex).name());
    return map;
}

} // namespace GitNaga
