#include "recent_projects.hpp"

#include <QDir>

#include <algorithm>
#include <QSettings>

namespace GitNaga {

namespace {
constexpr auto key = "recentProjects";
}

QString RecentProjects::settingsFile()
{
    return QSettings().fileName();
}

int RecentProjects::fuzzyScore(const QString &needle, const QString &candidate)
{
    const QString hay = candidate.toLower();
    const QString nick = needle.toLower();
    if (nick.isEmpty())
        return 1;
    int score = 0;
    int cursor = 0;
    int streak = 0;
    for (qsizetype i = 0; i < nick.size(); ++i) {
        const qsizetype at = hay.indexOf(nick.at(i), cursor);
        if (at < 0)
            return -1;
        score += at == cursor ? 3 + streak : 1;
        streak = at == cursor ? streak + 1 : 0;
        cursor = at + 1;
    }
    return score;
}

QVariantList RecentProjects::fuzzyMatch(const QString &needle)
{
    QVariantList matches;
    for (const auto &project : load()) {
        const int score = fuzzyScore(needle, project);
        if (score >= 0)
            matches.append(QVariantMap{
                { QStringLiteral("path"), project },
                { QStringLiteral("score"), score },
            });
    }
    std::sort(matches.begin(), matches.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("score")).toInt()
             > b.toMap().value(QStringLiteral("score")).toInt();
    });
    return matches;
}

QStringList RecentProjects::load()
{
    QSettings settings(settingsFile(), QSettings::IniFormat);
    QStringList projects = settings.value(key).toStringList();
    for (QString &project : projects)
        project = QDir::cleanPath(project);
    projects.removeAll(QString());
    projects.removeDuplicates();
    while (projects.size() > maximum)
        projects.removeLast();
    return projects;
}

void RecentProjects::record(const QString &worktree)
{
    const QString cleaned = QDir::cleanPath(worktree);
    if (cleaned.isEmpty())
        return;
    QStringList projects = load();
    projects.removeAll(cleaned);
    projects.prepend(cleaned);
    while (projects.size() > maximum)
        projects.removeLast();

    QSettings settings(settingsFile(), QSettings::IniFormat);
    settings.setValue(key, projects);
}

} // namespace GitNaga
