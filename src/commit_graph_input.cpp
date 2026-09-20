#include "commit_graph_item.hpp"

#include <QHoverEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace GitNaga {

void CommitGraphItem::hoverMoveEvent(QHoverEvent *event)
{
    if (m_dragging)
        return;
    const int row = rowAt(event->position().y());
    if (row == m_hoveredRow)
        return;
    m_hoveredRow = row;
    recomputeHighlight();
    emit hoveredRowChanged();
    update();
}

void CommitGraphItem::hoverLeaveEvent(QHoverEvent *event)
{
    Q_UNUSED(event);
    if (m_hoveredRow == -1)
        return;
    m_hoveredRow = -1;
    recomputeHighlight();
    emit hoveredRowChanged();
    update();
}

void CommitGraphItem::mousePressEvent(QMouseEvent *event)
{
    m_pressPosition = event->position();
    m_pressContentY = m_contentY;
    m_dragging = false;
    if (event->button() == Qt::RightButton) {
        const int row = rowAt(event->position().y());
        emit contextRequested(row, oidAt(row), event->globalPosition());
        event->accept();
        return;
    }
    event->accept();
}

void CommitGraphItem::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton))
        return;
    const qreal delta = event->position().y() - m_pressPosition.y();
    if (!m_dragging && std::abs(delta) > 4.0) {
        m_dragging = true;
        setKeepMouseGrab(true);
    }
    if (m_dragging)
        setContentY(m_pressContentY - delta);
    event->accept();
}

void CommitGraphItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setKeepMouseGrab(false);
        if (!m_dragging) {
            const int row = rowAt(event->position().y());
            if (row >= 0)
                emit commitClicked(row);
        }
    }
    m_dragging = false;
    event->accept();
}

void CommitGraphItem::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0) {
        event->ignore();
        return;
    }
    if (event->modifiers() & Qt::ControlModifier) {
        const qreal rowHeight = effectiveRowHeight();
        const qreal anchor = rowHeight > 0.0 ? (m_contentY + event->position().y()) / rowHeight : 0.0;
        setZoom(m_zoom * std::pow(1.0015, delta));
        setContentY(anchor * effectiveRowHeight() - event->position().y());
    } else {
        setContentY(m_contentY - (static_cast<qreal>(delta) / 120.0) * effectiveRowHeight());
    }
    event->accept();
}

void CommitGraphItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    clampContent();
    emit metricsChanged();
    update();
}

} // namespace GitNaga
