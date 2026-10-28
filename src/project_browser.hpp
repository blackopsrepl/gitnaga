#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

namespace GitNaga {

// The local filesystem as the Open Repository dialog browses it, plus the
// remembered projects. Nothing here needs an open repository; QML reaches it
// as `repository.projects`.
class ProjectBrowser final : public QObject
{
    Q_OBJECT

public:
    explicit ProjectBrowser(QObject *parent = nullptr);

    Q_INVOKABLE QVariantList directories(const QString &path) const;
    Q_INVOKABLE QString homeDirectory() const;
    Q_INVOKABLE bool looksLikeRepository(const QString &path) const;
    Q_INVOKABLE QStringList recentProjects() const;
    Q_INVOKABLE QVariantList fuzzyMatchProjects(const QString &needle) const;
};

} // namespace GitNaga
