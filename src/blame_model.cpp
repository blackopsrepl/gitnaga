#include "blame_model.hpp"

#include <QFutureWatcher>
#include <QSet>
#include <QtConcurrentRun>

namespace GitNaga {

BlameModel::BlameModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int BlameModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant BlameModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_lines.size())
        return {};
    const auto row = index.row();
    const auto &line = m_lines.at(row);
    const bool groupStart = row == 0 || m_lines.at(row - 1).oid != line.oid;
    switch (role) {
    case LineRole: return line.line;
    case TextRole: return line.text;
    case OidRole: return line.uncommitted() ? QString() : line.oid;
    case ShortOidRole: return line.uncommitted() ? QString() : line.oid.left(7);
    case AuthorRole: return line.uncommitted() ? tr("You") : line.author;
    case AgeRole: return line.uncommitted() ? tr("uncommitted") : detail::shortAge(line.authoredAt, m_now);
    case SummaryRole: return line.summary;
    case GroupStartRole: return groupStart;
    case GroupIndexRole: return m_groups.value(row);
    case UncommittedRole: return line.uncommitted();
    default: return {};
    }
}

QHash<int, QByteArray> BlameModel::roleNames() const
{
    return {
        { LineRole, "line" },       { TextRole, "text" },         { OidRole, "oid" },
        { ShortOidRole, "shortOid" }, { AuthorRole, "author" },    { AgeRole, "age" },
        { SummaryRole, "summary" }, { GroupStartRole, "groupStart" }, { GroupIndexRole, "groupIndex" },
        { UncommittedRole, "uncommitted" },
    };
}

void BlameModel::apply(QVector<BlameLine> lines)
{
    beginResetModel();
    m_lines = std::move(lines);
    m_now = QDateTime::currentDateTime();
    m_groups.resize(m_lines.size());
    QSet<QString> commits;
    int group = -1;
    for (qsizetype row = 0; row < m_lines.size(); ++row) {
        if (row == 0 || m_lines.at(row - 1).oid != m_lines.at(row).oid)
            ++group;
        m_groups[row] = group;
        commits.insert(m_lines.at(row).oid);
    }
    m_commitCount = static_cast<int>(commits.size());
    endResetModel();
}

void BlameModel::load(const QString &worktree, const QString &revision, const QString &path)
{
    if (m_cancel)
        m_cancel->store(true);
    m_cancel = std::make_shared<std::atomic_bool>(false);
    const auto generation = ++m_generation;
    m_path = path;
    m_revision = revision;
    m_error.clear();
    apply({});
    m_loading = true;
    emit loadingChanged();
    emit loaded();

    auto *watcher = new QFutureWatcher<GitResult<QVector<BlameLine>>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation] {
        watcher->deleteLater();
        if (generation != m_generation)
            return;
        auto result = watcher->result();
        m_error = result ? QString() : result.error().message;
        apply(result ? std::move(*result) : QVector<BlameLine>{});
        m_loading = false;
        emit loadingChanged();
        emit loaded();
    });
    watcher->setFuture(QtConcurrent::run([worktree, revision, path, cancel = m_cancel] {
        return blameFile(worktree, revision, path, cancel);
    }));
}

} // namespace GitNaga
