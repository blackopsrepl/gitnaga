#pragma once

#include <QVariant>
#include <QString>
#include <QStringList>

namespace GitNaga {

// Recently opened repositories, persisted in the user's settings. The list is
// newest first, path-cleaned, deduplicated and capped, so the switcher can
// offer it without ever growing unbounded.
class RecentProjects
{
public:
    static QStringList load();
    static void record(const QString &worktree);

    // Fuzzy subsequence match of `needle` against the remembered projects,
    // best first: [{path, score}]. A score of -1 means "no match" and such
    // entries are dropped; the needle may also be a plain path, which simply
    // scores every project.
    static QVariantList fuzzyMatch(const QString &needle);
    static int fuzzyScore(const QString &needle, const QString &candidate);

    // Exposed for tests: the default is the user's own settings file.
    static QString settingsFile();
    static constexpr int maximum = 12;
};

} // namespace GitNaga
