#include "commit_graph_item.hpp"



#include <algorithm>
#include <cmath>

namespace GitNaga {

namespace {
constexpr qreal kMinimumZoom = 0.45;
constexpr qreal kMaximumZoom = 2.8;
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
    m_rowByOid.clear();
    for (qsizetype row = 0; row < m_rows.size(); ++row)
        m_rowByOid.insert(m_rows.at(row).oid, static_cast<int>(row));
    recomputeHeadRow();
    if (m_selectedRow >= m_rows.size())
        setSelectedRow(-1);
    if (m_hoveredRow >= m_rows.size())
        m_hoveredRow = -1;
    // A new row set invalidates the cached ancestry: the same row index now
    // refers to a different commit, so force a rebuild instead of reusing a set
    // of object ids that no longer exists in the graph.
    m_highlightRow = -1;
    m_highlight.clear();
    recomputeHighlight();
    clampContent();
    emit metricsChanged();
    update();
}

graph::GraphStyle CommitGraphItem::style() const
{
    return graph::GraphStyle{ m_baseRowHeight * m_zoom, 22.0 * m_zoom, 26.0 };
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
    const qreal clamped = std::clamp(value, kMinimumZoom, kMaximumZoom);
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
    recomputeHighlight();
    emit selectedRowChanged();
    update();
}

void CommitGraphItem::recomputeHighlight()
{
    const int source = m_hoveredRow >= 0 ? m_hoveredRow : m_selectedRow;
    if (source == m_highlightRow)
        return;
    m_highlightRow = source;
    m_highlight.clear();
    if (source < 0 || source >= m_rows.size())
        return;

    QVector<int> pending{ source };
    m_highlight.insert(m_rows.at(source).oid);
    while (!pending.isEmpty()) {
        const int row = pending.takeLast();
        for (const auto &parent : m_rows.at(row).parents) {
            const auto found = m_rowByOid.constFind(parent);
            if (found == m_rowByOid.constEnd() || m_highlight.contains(parent))
                continue;
            m_highlight.insert(parent);
            pending.append(*found);
        }
    }
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

qreal CommitGraphItem::minimumZoom() { return kMinimumZoom; }

qreal CommitGraphItem::maximumZoom() { return kMaximumZoom; }

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
    if (rowHeight <= 0.0 || m_rows.isEmpty() || localY < 0.0)
        return -1;
    const int row = static_cast<int>(std::floor((m_contentY + localY) / rowHeight));
    return row >= 0 && row < m_rows.size() ? row : -1;
}

QString CommitGraphItem::oidAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row).oid : QString();
}

} // namespace GitNaga
