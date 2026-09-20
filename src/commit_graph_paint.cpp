#include "commit_graph_item.hpp"

#include "graph_palette.hpp"

#include <QBrush>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRadialGradient>

#include <algorithm>

namespace GitNaga {

using graph::darken;
using graph::laneColor;
using graph::lighten;
using graph::withAlpha;

namespace {
const QColor selectionColor(QStringLiteral("#f2fffa"));
const QColor headColor(QStringLiteral("#ffe9a8"));
constexpr int dimmedAlpha = 128;
} // namespace

void CommitGraphItem::paintEdges(QPainter *painter, int first, int last, const graph::GraphStyle &style) const
{
    const qreal scale = style.laneSpacing / 22.0;
    painter->setBrush(Qt::NoBrush);
    for (int row = first; row <= last; ++row) {
        const auto &commit = m_rows.at(row);
        const bool dimmed = !m_highlight.isEmpty() && !m_highlight.contains(commit.oid);
        const int alpha = dimmed ? dimmedAlpha : 255;
        for (const auto &edge : graph::edgesFor(commit, row, m_contentY, style)) {
            if (edge.path.size() < 2)
                continue;
            QPainterPath path(edge.path.first());
            for (qsizetype index = 1; index < edge.path.size(); ++index)
                path.lineTo(edge.path.at(index));

            const QColor from = withAlpha(laneColor(edge.fromLane), alpha);
            const QColor to = withAlpha(laneColor(edge.toLane), alpha);

            painter->setPen(QPen(withAlpha(QColor(4, 6, 11), dimmed ? 40 : 110),
                                 3.2 * scale, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawPath(path);

            QLinearGradient gradient(edge.path.first(), edge.path.last());
            gradient.setColorAt(0.0, from);
            gradient.setColorAt(1.0, to);
            painter->setPen(QPen(QBrush(gradient), 2.2 * scale, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawPath(path);
        }
    }
}

void CommitGraphItem::paintNode(QPainter *painter, const Commit &commit, const graph::Node &node, qreal radius,
                                bool isHead, bool selected, bool hovered) const
{
    const QColor base = laneColor(commit.lane);
    const qreal scale = style().laneSpacing / 22.0;
    const bool isMerge = commit.parents.size() > 1;
    const bool dimmed = !m_highlight.isEmpty() && !m_highlight.contains(commit.oid);
    const int alpha = dimmed ? dimmedAlpha : 255;
    const qreal nodeRadius = isMerge ? radius * 1.2 : radius;

    if (selected && !dimmed) {
        QRadialGradient glow(node.center, nodeRadius * 3.0);
        glow.setColorAt(0.0, withAlpha(base, 70));
        glow.setColorAt(1.0, withAlpha(base, 0));
        painter->setPen(Qt::NoPen);
        painter->setBrush(glow);
        painter->drawEllipse(node.center, nodeRadius * 3.0, nodeRadius * 3.0);
    }

    painter->setPen(QPen(withAlpha(darken(base, 0.5), alpha), 1.0 * scale));
    painter->setBrush(withAlpha(base, alpha));
    painter->drawEllipse(node.center, nodeRadius, nodeRadius);

    QColor ring = lighten(base, 0.25);
    qreal ringWidth = 1.8 * scale;
    if (selected && !dimmed)
        ring = selectionColor;
    else if (hovered && !dimmed)
        ring = lighten(base, 0.55);
    else if (isHead)
        ringWidth = 2.2 * scale;
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(withAlpha(ring, dimmed ? qMin(alpha, 150) : 255), ringWidth));
    painter->drawEllipse(node.center, nodeRadius + 2.4 * scale, nodeRadius + 2.4 * scale);
}

void CommitGraphItem::paint(QPainter *painter)
{
    if (m_rows.isEmpty() || effectiveRowHeight() <= 0.0)
        return;
    painter->setRenderHint(QPainter::Antialiasing, true);

    const auto st = style();
    const int first = graph::firstVisibleRow(m_contentY, height(), st);
    const int last = graph::lastVisibleRow(m_contentY, height(), static_cast<int>(m_rows.size()), st);
    if (last < first)
        return;

    for (int row = first; row <= last; ++row) {
        const qreal y = graph::rowCenterY(row, m_contentY, st) - st.rowHeight / 2.0;
        const QColor band = laneColor(m_rows.at(row).lane);
        const bool dimmed = !m_highlight.isEmpty() && !m_highlight.contains(m_rows.at(row).oid);
        int alpha = 26;
        if (row == m_selectedRow)
            alpha = 56;
        else if (row == m_hoveredRow)
            alpha = 38;
        if (dimmed)
            alpha = 10;
        painter->fillRect(QRectF(0.0, y, width(), st.rowHeight), withAlpha(band, alpha));
    }

    paintEdges(painter, first, last, st);

    const qreal radius = std::clamp(6.0 * (st.laneSpacing / 22.0), 3.0, 12.0);
    for (int row = first; row <= last; ++row) {
        const auto &commit = m_rows.at(row);
        const auto node = graph::nodeFor(commit, row, m_contentY, st);
        paintNode(painter, commit, node, radius, row == m_headRow, row == m_selectedRow, row == m_hoveredRow);
    }
}

} // namespace GitNaga
