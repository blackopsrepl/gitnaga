#include "commit_graph_item.hpp"


#include <QHoverEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace GitNaga {

namespace {
constexpr qreal minimumZoom = 0.45;
constexpr qreal maximumZoom = 2.8;
}

CommitGraphItem::CommitGraphItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setRenderTarget(QQuickPaintedItem::Image);
    setAcceptHoverEvents(true);
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

CommitModel *CommitGraphItem::model() const { return m_model; }

void CommitGraphItem::setModel(CommitModel *model)
{
    if (m_model == model)
        return;
    if (m_model)
        disconnect(m_model, nullptr, this, nullptr);
    m_model = model;
    if (m_model) {
        connect(m_model, &QAbstractItemModel::modelReset, this, &CommitGraphItem::synchronizeRows);
        connect(m_model, &QAbstractItemModel::rowsInserted, this, &CommitGraphItem::synchronizeRows);
        connect(m_model, &QAbstractItemModel::rowsRemoved, this, &CommitGraphItem::synchronizeRows);
    }
    synchronizeRows();
    emit modelChanged();
}

void CommitGraphItem::synchronizeRows()
{
    m_rows = m_model ? m_model->commits() : QVector<Commit>{};
    m_maximumLane = graph::maximumLane(m_rows);
    recomputeHeadRow();
    if (m_selectedRow >= m_rows.size())
        setSelectedRow(-1);
    if (m_hoveredRow >= m_rows.size())
        m_hoveredRow = -1;
    clampContent();
    emit metricsChanged();
    update();
}

graph::GraphStyle CommitGraphItem::style() const
{
    return graph::GraphStyle{ m_baseRowHeight * m_zoom, 22.0 * m_zoom, 28.0 };
}

qreal CommitGraphItem::contentY() const { return m_contentY; }

void CommitGraphItem::setContentY(qreal value)
{
    const qreal clamped = std::clamp(value, 0.0, maxContentY());
    if (qFuzzyCompare(m_contentY + 1.0, clamped + 1.0))
        return;
    m_contentY = clamped;
    update();
    emit contentYChanged();
}

qreal CommitGraphItem::baseRowHeight() const { return m_baseRowHeight; }

void CommitGraphItem::setBaseRowHeight(qreal value)
{
    if (qFuzzyCompare(m_baseRowHeight, value) || value <= 0.0)
        return;
    m_baseRowHeight = value;
    clampContent();
    emit baseRowHeightChanged();
    emit metricsChanged();
    update();
}

qreal CommitGraphItem::zoom() const { return m_zoom; }

void CommitGraphItem::setZoom(qreal value)
{
    const qreal clamped = std::clamp(value, minimumZoom, maximumZoom);
    if (qFuzzyCompare(m_zoom, clamped))
        return;
    m_zoom = clamped;
    clampContent();
    emit zoomChanged();
    emit metricsChanged();
    update();
}

int CommitGraphItem::selectedRow() const { return m_selectedRow; }

void CommitGraphItem::setSelectedRow(int row)
{
    const int normalized = row < 0 || row >= m_rows.size() ? -1 : row;
    if (m_selectedRow == normalized)
        return;
    m_selectedRow = normalized;
    emit selectedRowChanged();
    update();
}

int CommitGraphItem::hoveredRow() const { return m_hoveredRow; }

QString CommitGraphItem::headOid() const { return m_headOid; }

void CommitGraphItem::setHeadOid(const QString &oid)
{
    if (m_headOid == oid)
        return;
    m_headOid = oid;
    recomputeHeadRow();
    emit headOidChanged();
    update();
}

void CommitGraphItem::recomputeHeadRow()
{
    m_headRow = -1;
    if (m_headOid.isEmpty())
        return;
    for (qsizetype row = 0; row < m_rows.size(); ++row) {
        if (m_rows.at(row).oid == m_headOid) {
            m_headRow = static_cast<int>(row);
            return;
        }
    }
}

qreal CommitGraphItem::effectiveRowHeight() const { return m_baseRowHeight * m_zoom; }
qreal CommitGraphItem::laneWidth() const { return graph::laneAreaWidth(m_maximumLane, style()); }
qreal CommitGraphItem::contentHeight() const { return static_cast<qreal>(m_rows.size()) * effectiveRowHeight(); }
qreal CommitGraphItem::maxContentY() const { return std::max(0.0, contentHeight() - height()); }

void CommitGraphItem::clampContent() { setContentY(m_contentY); }

void CommitGraphItem::zoomIn() { setZoom(m_zoom * 1.18); }
void CommitGraphItem::zoomOut() { setZoom(m_zoom / 1.18); }
void CommitGraphItem::resetZoom() { setZoom(1.0); }

void CommitGraphItem::scrollToRow(int row)
{
    setContentY(static_cast<qreal>(row) * effectiveRowHeight() + effectiveRowHeight() / 2.0 - height() / 2.0);
}

void CommitGraphItem::ensureVisible(int row)
{
    if (row < 0)
        return;
    const qreal rowHeight = effectiveRowHeight();
    const qreal top = static_cast<qreal>(row) * rowHeight;
    const qreal bottom = top + rowHeight;
    if (top < m_contentY)
        setContentY(top - rowHeight);
    else if (bottom > m_contentY + height())
        setContentY(bottom - height() + rowHeight);
}

int CommitGraphItem::rowAt(qreal localY) const
{
    const qreal rowHeight = effectiveRowHeight();
    if (rowHeight <= 0.0 || m_rows.isEmpty())
        return -1;
    return std::clamp(static_cast<int>(std::floor((m_contentY + localY) / rowHeight)), 0,
                      static_cast<int>(m_rows.size()) - 1);
}

QString CommitGraphItem::oidAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row).oid : QString();
}

void CommitGraphItem::hoverMoveEvent(QHoverEvent *event)
{
    const int row = rowAt(event->position().y());
    if (row == m_hoveredRow)
        return;
    m_hoveredRow = row;
    emit hoveredRowChanged();
    update();
}

void CommitGraphItem::hoverLeaveEvent(QHoverEvent *event)
{
    Q_UNUSED(event);
    if (m_hoveredRow == -1)
        return;
    m_hoveredRow = -1;
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
