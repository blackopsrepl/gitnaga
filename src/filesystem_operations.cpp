#include "repository_controller.hpp"

#include "recent_projects.hpp"

#include <QDir>
#include <QFileInfo>
#include <QVariantMap>

namespace GitNaga {

QVariantList RepositoryController::directories(const QString &path) const
{
    QVariantList entries;
    QDir directory(path.isEmpty() ? QDir::homePath() : path);
    const auto infos = directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                               QDir::Name | QDir::IgnoreCase);
    for (const auto &info : infos) {
        if (!info.isDir())
            continue;
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), info.fileName());
        entry.insert(QStringLiteral("path"), info.absoluteFilePath());
        entry.insert(QStringLiteral("repository"),
                     QFileInfo::exists(QDir(info.absoluteFilePath()).filePath(QStringLiteral(".git")))
                         || QFileInfo::exists(QDir(info.absoluteFilePath()).filePath(QStringLiteral("HEAD"))));
        entries.append(entry);
    }
    return entries;
}

QString RepositoryController::homeDirectory() const
{
    return QDir::homePath();
}

QStringList RepositoryController::recentProjects() const
{
    return RecentProjects::load();
}

QVariantList RepositoryController::fuzzyMatchProjects(const QString &needle) const
{
    return RecentProjects::fuzzyMatch(needle);
}

bool RepositoryController::looksLikeRepository(const QString &path) const
{
    if (path.isEmpty())
        return false;
    const QDir directory(path);
    return QFileInfo::exists(directory.filePath(QStringLiteral(".git")))
           || (QFileInfo::exists(directory.filePath(QStringLiteral("HEAD")))
               && QFileInfo::exists(directory.filePath(QStringLiteral("objects"))));
}

} // namespace GitNaga
