#include "repository_controller.hpp"

#include "graph_palette.hpp"

#include <QSet>
#include <QVariantMap>

#include <algorithm>

namespace GitNaga {

void RepositoryController::selectOid(const QString &oid)
{
    for (qsizetype row = 0; row < m_repository.commits.size(); ++row) {
        if (m_repository.commits.at(row).oid == oid) {
            selectCommit(static_cast<int>(row));
            return;
        }
    }
}

void RepositoryController::rebuildReferences()
{
    QVariantList references;
    QSet<QString> seen;
    for (const auto &commit : m_repository.commits) {
        for (const auto &raw : commit.refs) {
            QString kind = QStringLiteral("local");
            QString name = raw;
            if (raw.startsWith(QStringLiteral("⇄ "))) {
                kind = QStringLiteral("remote");
                name = raw.mid(2);
            } else if (raw.startsWith(QStringLiteral("# "))) {
                kind = QStringLiteral("tag");
                name = raw.mid(2);
            }
            const QString key = kind + QLatin1Char('/') + name;
            if (seen.contains(key))
                continue;
            seen.insert(key);
            QVariantMap entry;
            entry.insert(QStringLiteral("name"), name);
            entry.insert(QStringLiteral("kind"), kind);
            entry.insert(QStringLiteral("oid"), commit.oid);
            entry.insert(QStringLiteral("shortOid"), commit.oid.left(8));
            entry.insert(QStringLiteral("subject"), commit.subject);
            entry.insert(QStringLiteral("color"), graph::laneColor(commit.colorIndex).name());
            references.append(entry);
        }
    }
    const auto rank = [](const QString &kind) {
        if (kind == QStringLiteral("local"))
            return 0;
        if (kind == QStringLiteral("remote"))
            return 1;
        return 2;
    };
    std::sort(references.begin(), references.end(), [&rank](const QVariant &left, const QVariant &right) {
        const auto leftMap = left.toMap();
        const auto rightMap = right.toMap();
        const int leftRank = rank(leftMap.value(QStringLiteral("kind")).toString());
        const int rightRank = rank(rightMap.value(QStringLiteral("kind")).toString());
        if (leftRank != rightRank)
            return leftRank < rightRank;
        return leftMap.value(QStringLiteral("name")).toString().compare(
                   rightMap.value(QStringLiteral("name")).toString(), Qt::CaseInsensitive) < 0;
    });
    m_references = references;
    emit referencesChanged();
}

} // namespace GitNaga
