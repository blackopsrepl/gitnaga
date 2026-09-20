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
using graph::mix;
using graph::withAlpha;

namespace {
const QColor selectionColor(QStringLiteral("#eafff6"));
const QColor accentColor(QStringLiteral("#34d399"));
}

void CommitGraphItem::paintEdges(QPainter *painter, int first, int last, const graph::GraphStyle &style) const
{
    const qreal scale = style.laneSpacing / 22.0;
    painter->setBrush(Qt::NoBrush);
    for (int row = first; row <= last; ++row) {
        const auto &commit = m_rows.at(row);
        for (const auto &edge : graph::edgesFor(commit, row, m_contentY, style)) {
            if (edge.path.size() < 2)
                continue;
            QPainterPath path(edge.path.first());
            for (qsizetype index = 1; index < edge.path.size(); ++index)
                path.lineTo(edge.path.at(index));

            painter->setPen(QPen(QColor(4, 6, 11, 170), 3.8 * scale, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawPath(path);

            const QColor from = laneColor(edge.fromLane);
            const QColor to = laneColor(edge.toLane);
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

    if (selected) {
        QRadialGradient glow(node.center, radius * 3.4);
        glow.setColorAt(0.0, withAlpha(base, 90));
        glow.setColorAt(1.0, withAlpha(base, 0));
        painter->setPen(Qt::NoPen);
        painter->setBrush(glow);
        painter->drawEllipse(node.center, radius * 3.4, radius * 3.4);
    }

    QRadialGradient body(node.center + QPointF(-radius * 0.3, -radius * 0.35), radius * 1.7, node.center);
    body.setColorAt(0.0, mix(base, QColor(QStringLiteral("#ffffff")), 0.3));
    body.setColorAt(0.7, base);
    body.setColorAt(1.0, darken(base, 0.4));
    painter->setPen(QPen(withAlpha(darken(base, 0.55), 230), 1.0 * scale));
    painter->setBrush(body);
    painter->drawEllipse(node.center, radius, radius);

    painter->setBrush(Qt::NoBrush);
    if (isMerge) {
        painter->setPen(QPen(base, 2.2 * scale));
        painter->drawEllipse(node.center, radius + 2.6 * scale, radius + 2.6 * scale);
    }
    if (isHead) {
        painter->setPen(QPen(withAlpha(lighten(base, 0.55), 220), 1.2 * scale));
        painter->drawEllipse(node.center, radius + 4.8 * scale, radius + 4.8 * scale);
    }
    if (selected) {
        painter->setPen(QPen(selectionColor, 1.6 * scale));
        painter->drawEllipse(node.center, radius + 3.0 * scale, radius + 3.0 * scale);
        painter->setPen(QPen(withAlpha(accentColor, 150), 1.1 * scale));
        painter->drawEllipse(node.center, radius + 5.6 * scale, radius + 5.6 * scale);
    } else if (hovered) {
        painter->setPen(QPen(withAlpha(lighten(base, 0.45), 225), 1.5 * scale));
        painter->drawEllipse(node.center, radius + 3.0 * scale, radius + 3.0 * scale);
    }
}

void CommitGraphItem::paint(QPainter *painter)
{
    if (m_rows.isEmpty() || effectiveRowHeight() <= 0.0)
        return;
    painter->setRenderHint(QPainter::Antialiasing, true);

    const auto st = style();
    const qreal scale = st.laneSpacing / 22.0;
    const int first = graph::firstVisibleRow(m_contentY, height(), st);
    const int last = graph::lastVisibleRow(m_contentY, height(), static_cast<int>(m_rows.size()), st);
    if (last < first)
        return;

    for (int row = first; row <= last; ++row) {
        const qreal y = graph::rowCenterY(row, m_contentY, st) - st.rowHeight / 2.0;
        const QColor band = laneColor(m_rows.at(row).lane);
        int alpha = 30;
        if (row == m_selectedRow)
            alpha = 64;
        else if (row == m_hoveredRow)
            alpha = 44;
        painter->fillRect(QRectF(0.0, y, width(), st.rowHeight), withAlpha(band, alpha));
    }

    painter->setBrush(Qt::NoBrush);
    paintEdges(painter, first, last, st);

    const qreal radius = std::clamp(6.5 * scale, 3.5, 13.0);
    for (int row = first; row <= last; ++row) {
        const auto &commit = m_rows.at(row);
        const auto node = graph::nodeFor(commit, row, m_contentY, st);
        paintNode(painter, commit, node, radius, row == m_headRow, row == m_selectedRow, row == m_hoveredRow);
    }
}

} // namespace GitNaga
