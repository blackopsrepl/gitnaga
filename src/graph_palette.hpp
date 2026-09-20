#pragma once

#include <QColor>
#include <QString>

#include <array>
#include <cstddef>

namespace GitNaga::graph {

inline const std::array<QColor, 10> &palette()
{
    static const std::array<QColor, 10> colors = {
        QColor(QStringLiteral("#a970ff")),
        QColor(QStringLiteral("#3d91f4")),
        QColor(QStringLiteral("#e350b0")),
        QColor(QStringLiteral("#4fbf67")),
        QColor(QStringLiteral("#e8c545")),
        QColor(QStringLiteral("#e05252")),
        QColor(QStringLiteral("#45c5e0")),
        QColor(QStringLiteral("#f07a3d")),
        QColor(QStringLiteral("#b78af5")),
        QColor(QStringLiteral("#67d9a0")),
    };
    return colors;
}

inline QColor laneColor(int lane)
{
    const int count = static_cast<int>(palette().size());
    const int normalized = ((lane % count) + count) % count;
    return palette().at(static_cast<std::size_t>(normalized));
}

inline QColor mix(const QColor &from, const QColor &to, qreal amount)
{
    const qreal t = amount < 0.0 ? 0.0 : (amount > 1.0 ? 1.0 : amount);
    return QColor::fromRgbF(static_cast<float>(from.redF() + (to.redF() - from.redF()) * t),
                            static_cast<float>(from.greenF() + (to.greenF() - from.greenF()) * t),
                            static_cast<float>(from.blueF() + (to.blueF() - from.blueF()) * t),
                            static_cast<float>(from.alphaF() + (to.alphaF() - from.alphaF()) * t));
}

inline QColor lighten(const QColor &color, qreal amount)
{
    return mix(color, QColor(QStringLiteral("#ffffff")), amount);
}

inline QColor darken(const QColor &color, qreal amount)
{
    return mix(color, QColor(QStringLiteral("#05070c")), amount);
}

inline QColor withAlpha(const QColor &color, int alpha)
{
    QColor result = color;
    result.setAlpha(alpha);
    return result;
}

} // namespace GitNaga::graph
