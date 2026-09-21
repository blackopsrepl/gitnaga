#pragma once

#include <QColor>
#include <QString>

#include <array>
#include <cstddef>

namespace GitNaga::graph {

// Lane colours: neon on the near-black window, ordered so that neighbouring
// lanes never look alike (adjacent indices are what a reader compares side by
// side) and so the first branch leads with green rather than with red, which
// the interface reserves for destructive actions. Chosen by maximising the
// minimum CIELab distance subject to every entry holding at least 4.5:1
// against the window background and staying high-chroma: min pairwise dE 26,
// min neighbouring dE 100, against dE 20 and 44 for the previous set, where
// two greens at dE 20 and two violets at dE 21 made distinct branches read as
// the same colour.
inline const std::array<QColor, 12> &palette()
{
    static const std::array<QColor, 12> colors = {
        QColor(QStringLiteral("#22bf70")), // emerald
        QColor(QStringLiteral("#da2fa1")), // hot pink
        QColor(QStringLiteral("#07edc7")), // aqua
        QColor(QStringLiteral("#da462f")), // flame
        QColor(QStringLiteral("#25d1f4")), // electric cyan
        QColor(QStringLiteral("#bb37e6")), // electric violet
        QColor(QStringLiteral("#bfaa22")), // acid yellow
        QColor(QStringLiteral("#5c8dff")), // electric blue
        QColor(QStringLiteral("#5aad1f")), // lime
        QColor(QStringLiteral("#f5005a")), // magenta red
        QColor(QStringLiteral("#7367f4")), // indigo
        QColor(QStringLiteral("#d07a25")), // amber
    };
    return colors;
}

// Pointer highlight. Deliberately neutral and low-chroma: hue is reserved for
// branch identity, so hovering the green line cannot read as a green state.
inline QColor pointerColor()
{
    return QColor(QStringLiteral("#cfd9e8"));
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
