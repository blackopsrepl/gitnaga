#pragma once

#include "commit_model.hpp"
#include "graph_geometry.hpp"

#include <QPointF>
#include <QPointer>
#include <QQuickPaintedItem>

namespace GitNaga {

class CommitGraphItem : public QQuickPaintedItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(CommitGraph)
    Q_PROPERTY(CommitModel *model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(qreal contentY READ contentY WRITE setContentY NOTIFY contentYChanged)
    Q_PROPERTY(qreal baseRowHeight READ baseRowHeight WRITE setBaseRowHeight NOTIFY baseRowHeightChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(int selectedRow READ selectedRow WRITE setSelectedRow NOTIFY selectedRowChanged)
    Q_PROPERTY(int hoveredRow READ hoveredRow NOTIFY hoveredRowChanged)
    Q_PROPERTY(QString headOid READ headOid WRITE setHeadOid NOTIFY headOidChanged)
    Q_PROPERTY(qreal effectiveRowHeight READ effectiveRowHeight NOTIFY metricsChanged)
    Q_PROPERTY(qreal laneWidth READ laneWidth NOTIFY metricsChanged)
    Q_PROPERTY(qreal contentHeight READ contentHeight NOTIFY metricsChanged)
    Q_PROPERTY(qreal maxContentY READ maxContentY NOTIFY metricsChanged)

public:
    explicit CommitGraphItem(QQuickItem *parent = nullptr);

    CommitModel *model() const;
    void setModel(CommitModel *model);
    qreal contentY() const;
    void setContentY(qreal contentY);
    qreal baseRowHeight() const;
    void setBaseRowHeight(qreal value);
    qreal zoom() const;
    void setZoom(qreal value);
    int selectedRow() const;
    void setSelectedRow(int row);
    int hoveredRow() const;
    QString headOid() const;
    void setHeadOid(const QString &oid);
    qreal effectiveRowHeight() const;
    qreal laneWidth() const;
    qreal contentHeight() const;
    qreal maxContentY() const;

    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void resetZoom();
    Q_INVOKABLE void scrollToRow(int row);
    Q_INVOKABLE void ensureVisible(int row);

signals:
    void modelChanged();
    void contentYChanged();
    void baseRowHeightChanged();
    void zoomChanged();
    void selectedRowChanged();
    void hoveredRowChanged();
    void headOidChanged();
    void metricsChanged();
    void commitClicked(int row);
    void contextRequested(int row, const QString &oid, const QPointF &globalPosition);

protected:
    void paint(QPainter *painter) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    graph::GraphStyle style() const;
    int rowAt(qreal localY) const;
    QString oidAt(int row) const;
    void recomputeHeadRow();
    void clampContent();
    void synchronizeRows();
    void paintEdges(QPainter *painter, int first, int last, const graph::GraphStyle &style) const;
    void paintNode(QPainter *painter, const Commit &commit, const graph::Node &node, qreal radius, bool isHead, bool selected, bool hovered) const;

    QPointer<CommitModel> m_model;
    QVector<Commit> m_rows;
    qreal m_contentY = 0.0;
    qreal m_baseRowHeight = 52.0;
    qreal m_zoom = 1.0;
    int m_selectedRow = -1;
    int m_hoveredRow = -1;
    int m_maximumLane = 0;
    int m_headRow = -1;
    QString m_headOid;
    bool m_dragging = false;
    QPointF m_pressPosition;
    qreal m_pressContentY = 0.0;
};

} // namespace GitNaga
