#include "avatar_provider.hpp"

#include <QColor>
#include <QCryptographicHash>
#include <QDir>
#include <QFont>
#include <QPainter>
#include <QRegularExpression>
#include <QStandardPaths>

#include <array>

namespace GitNaga {
namespace {

// A curated muted palette keeps monograms distinct from each other while the
// branch-coloured ring stays the dominant signal on the node.
const std::array<QColor, 10> avatarColors = {
    QColor(QStringLiteral("#3f5b8b")), QColor(QStringLiteral("#6b4f8f")),
    QColor(QStringLiteral("#2f7d74")), QColor(QStringLiteral("#8a5a3c")),
    QColor(QStringLiteral("#4f6b3a")), QColor(QStringLiteral("#8c4a5a")),
    QColor(QStringLiteral("#3c6e91")), QColor(QStringLiteral("#7a5c9e")),
    QColor(QStringLiteral("#2f6f4f")), QColor(QStringLiteral("#6d5a2f")),
};

QByteArray digest(const QString &email)
{
    const QString normalized = email.trimmed().toLower();
    return QCryptographicHash::hash(normalized.toUtf8(), QCryptographicHash::Sha256);
}

QString initialsFor(const QString &name, const QString &email)
{
    const QString source = name.trimmed().isEmpty() ? email.section(QLatin1Char('@'), 0, 0) : name.trimmed();
    const QStringList parts = source.split(QRegularExpression(QStringLiteral("[\\s._-]+")), Qt::SkipEmptyParts);
    if (parts.size() >= 2)
        return (parts.first().left(1) + parts.last().left(1)).toUpper();
    if (parts.size() == 1)
        return parts.first().left(2).toUpper();
    return QStringLiteral("?");
}

QImage scaleTo(const QImage &source, int size)
{
    if (source.isNull() || source.width() == size)
        return source;
    return source.scaled(size, size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

} // namespace

QString AvatarProvider::cacheDirectory()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return QDir(base).filePath(QStringLiteral("avatars"));
}

QString AvatarProvider::cachedAvatarPath(const QString &email)
{
    return QDir(cacheDirectory()).filePath(QString::fromLatin1(digest(email).toHex()) + QStringLiteral(".png"));
}

QImage AvatarProvider::loadCached(const QString &email, int size) const
{
    if (email.isEmpty())
        return {};
    QImage image;
    if (!image.load(cachedAvatarPath(email)))
        return {};
    return scaleTo(image, size);
}

QImage AvatarProvider::generate(const QString &email, const QString &name, int size)
{
    const QByteArray hash = digest(email.isEmpty() ? name : email);
    const auto colorIndex = static_cast<std::size_t>(static_cast<unsigned char>(hash.at(0)) % avatarColors.size());

    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(avatarColors.at(colorIndex));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(qMax(8, static_cast<int>(size * 0.42)));
    painter.setFont(font);
    painter.setPen(QColor(255, 255, 255, 235));
    painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, initialsFor(name, email));
    painter.end();
    return image;
}

QImage AvatarProvider::avatar(const QString &email, const QString &name, int size) const
{
    if ((email.isEmpty() && name.isEmpty()) || size <= 0)
        return {};
    const QString key = email + QLatin1Char('|') + name + QLatin1Char('|') + QString::number(size);
    const auto cached = m_cache.constFind(key);
    if (cached != m_cache.constEnd())
        return *cached;

    QImage image = loadCached(email, size);
    if (image.isNull())
        image = generate(email, name, size);
    m_cache.insert(key, image);
    return image;
}

} // namespace GitNaga
