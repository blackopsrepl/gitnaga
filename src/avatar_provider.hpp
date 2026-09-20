#pragma once

#include <QHash>
#include <QImage>
#include <QString>

namespace GitNaga {

// Supplies a per-author avatar without any network access.
//
// 1. If an image exists at <cache>/avatars/<sha256(email)>.png it is used, so a
//    user can drop real photos in.
// 2. Otherwise a deterministic monogram is drawn from the author name and email
//    colour, so the same author always renders the same avatar on every machine.
//    A monogram stays legible at the node size, unlike a small block identicon.
class AvatarProvider
{
public:
    QImage avatar(const QString &email, const QString &name, int size) const;

    static QString cacheDirectory();
    static QString cachedAvatarPath(const QString &email);

private:
    QImage loadCached(const QString &email, int size) const;
    static QImage generate(const QString &email, const QString &name, int size);

    mutable QHash<QString, QImage> m_cache;
};

} // namespace GitNaga
