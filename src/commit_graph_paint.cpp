#include "commit_graph_item.hpp"

#include "graph_palette.hpp"

#include <QBrush>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QQuickWindow>

#include <algorithm>
#include <cmath>

namespace GitNaga {

using graph::darken;
using graph::laneColor;
using graph::lighten;
using graph::withAlpha;

namespace {
const QColor selectionColor(QStringLiteral("#f2fffa"));
const QColor headColor(QStringLiteral("#ffe9a8"));
// Pointer cue, shared with the palette so the rule stays testable: hue is for
// branch identity, never for the pointer.
const QColor hoverColor = graph::pointerColor();
constexpr int dimmedAlpha = 128;
// Rows over which the trace ramps in and out. The ramp is what makes the
// hand-off between a branch and the line it forked from a crossfade instead of
// a step.
constexpr int emphasisFadeRows = 6;
} // namespace

void CommitGraphItem::paintEdges(QPainter *painter, int first, int last, const graph::GraphStyle &style) const
{
    const qreal scale = style.laneSpacing / 22.0;
    painter->setBrush(Qt::NoBrush);
    for (int row = first; row <= last; ++row) {
        const auto &commit = m_rows.at(row);
        for (const auto &edge : graph::edgesFor(commit, row, m_contentY, style)) {
            if (edge.path.size() < 2)
                continue;

            // Dimming follows the *line*, not the row: a lane that merely
            // passes an emphasised row belongs to another branch and must stay
            // dim, otherwise foreign branches light up in patches wherever the
            // trace crosses them. An edge leaving this commit belongs to the
            // line this commit sits on; a passing edge to the line it carries.
            const int line = edge.fromLane == commit.lane ? commit.colorIndex : edge.fromColor;
            const qreal top = graph::spanStrength(m_emphasisSpans, line, row, emphasisFadeRows);
            const qreal bottom = graph::spanStrength(m_emphasisSpans, line, row + 1, emphasisFadeRows);
            const auto alphaAt = [](qreal strength) {
                return static_cast<int>(std::lround(dimmedAlpha + (255 - dimmedAlpha) * strength));
            };
            const QColor from = withAlpha(laneColor(edge.fromColor), alphaAt(top));
            const QColor to = withAlpha(laneColor(edge.toColor), alphaAt(bottom));

            QPainterPath path(edge.path.first());
            for (qsizetype index = 1; index < edge.path.size(); ++index)
                path.lineTo(edge.path.at(index));

            const int shadowAlpha = static_cast<int>(std::lround(40 + 70 * (top + bottom) / 2.0));
            painter->setPen(QPen(withAlpha(QColor(4, 6, 11), shadowAlpha),
                                 3.2 * scale, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->drawPath(path);

            // The gradient carries both the lane-colour transition and the
            // trace strength, so brightness ramps continuously along the line.
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
    const QColor base = laneColor(commit.colorIndex);
    const qreal scale = style().laneSpacing / 22.0;
    const bool isMerge = commit.parents.size() > 1;
    const bool dimmed = !m_highlight.isEmpty() && !m_highlight.contains(commit.oid);
    const int alpha = dimmed ? dimmedAlpha : 255;
    const qreal nodeRadius = isMerge ? radius * 1.2 : radius;

    if (!m_workInProgressOid.isEmpty() && commit.oid == m_workInProgressOid) {
        // Uncommitted work: a hollow dashed disc instead of an author avatar,
        // so it reads as pending rather than as a recorded commit. Selection
        // reads from the dashed ring's colour, not from a glow.
        const QColor pending = selected && !dimmed ? selectionColor
                             : hovered && !dimmed ? hoverColor
                                                 : QColor(151, 161, 180);
        painter->setBrush(withAlpha(QColor(11, 17, 32), dimmed ? qMin(alpha, 90) : 200));
        painter->setPen(QPen(withAlpha(pending, alpha), 1.4 * scale, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawEllipse(node.center, nodeRadius, nodeRadius);
        painter->setBrush(withAlpha(pending, alpha));
        painter->setPen(Qt::NoPen);
        const qreal dotRadius = std::max(1.2, nodeRadius * 0.22);
        painter->drawEllipse(node.center, dotRadius, dotRadius);
        return;
    }

    const qreal ratio = window() ? window()->devicePixelRatio() : 1.0;
    const int avatarSize = std::max(12, static_cast<int>(std::lround(nodeRadius * 2.0 * ratio)));
    const QImage avatar = m_avatars.avatar(commit.authorEmail, commit.author, avatarSize);
    if (!avatar.isNull()) {
        painter->save();
        painter->setOpacity(dimmed ? 0.5 : 1.0);
        QPainterPath clip;
        clip.addEllipse(node.center, nodeRadius, nodeRadius);
        painter->setClipPath(clip);
        painter->drawImage(QRectF(node.center.x() - nodeRadius, node.center.y() - nodeRadius,
                                  nodeRadius * 2.0, nodeRadius * 2.0),
                           avatar);
        painter->restore();
    } else {
        painter->setPen(Qt::NoPen);
        painter->setBrush(withAlpha(base, alpha));
        painter->drawEllipse(node.center, nodeRadius, nodeRadius);
    }

    painter->setPen(QPen(withAlpha(darken(base, 0.5), alpha), 1.0 * scale));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(node.center, nodeRadius, nodeRadius);

    QColor ring = lighten(base, 0.25);
    qreal ringWidth = 1.8 * scale;
    if (selected && !dimmed) {
        ring = selectionColor;
        ringWidth = 2.8 * scale;
    } else if (hovered && !dimmed) {
        ring = hoverColor;
        ringWidth = 2.2 * scale;
    } else if (isHead) {
        ringWidth = 2.2 * scale;
    }
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
        // Branch colour is identity, so it tints its line's rows and the
        // selected row; the pointer's row takes a neutral wash so hover never
        // reads as a brighter version of the branch hue.
        const bool hoveredRow = row == m_hoveredRow && row != m_selectedRow;
        const QColor band = hoveredRow ? hoverColor : laneColor(m_rows.at(row).colorIndex);
        const bool dimmed = !m_highlight.isEmpty() && !m_highlight.contains(m_rows.at(row).oid);
        int alpha = 26;
        if (row == m_selectedRow)
            alpha = 56;
        else if (hoveredRow)
            alpha = 38;
        if (dimmed)
            alpha = 10;
        painter->fillRect(QRectF(0.0, y, width(), st.rowHeight), withAlpha(band, alpha));
    }

    paintEdges(painter, first, last, st);

    const qreal radius = std::clamp(8.0 * (st.laneSpacing / 22.0), 4.0, 16.0);
    for (int row = first; row <= last; ++row) {
        const auto &commit = m_rows.at(row);
        const auto node = graph::nodeFor(commit, row, m_contentY, st);
        paintNode(painter, commit, node, radius, row == m_headRow, row == m_selectedRow, row == m_hoveredRow);
    }
}

} // namespace GitNaga
