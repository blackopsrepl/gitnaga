#pragma once

#include "domain.hpp"

#include <QPointF>
#include <QVector>

namespace GitNaga::graph {

struct GraphStyle {
    qreal rowHeight = 36.0;
    qreal laneSpacing = 22.0;
    qreal leftPadding = 28.0;
};

struct Node {
    QPointF center;
    qreal radius = 7.0;
    int lane = 0;
};

struct Edge {
    QVector<QPointF> path;
    int fromLane = 0;
    int toLane = 0;
};

qreal laneX(int lane, const GraphStyle &style);
qreal rowCenterY(int row, qreal contentY, const GraphStyle &style);
qreal laneAreaWidth(int maximumLane, const GraphStyle &style);

Node nodeFor(const Commit &commit, int row, qreal contentY, const GraphStyle &style);
QVector<Edge> edgesFor(const Commit &commit, int row, qreal contentY, const GraphStyle &style, int samples = 18);

int firstVisibleRow(qreal contentY, qreal viewportHeight, const GraphStyle &style);
int lastVisibleRow(qreal contentY, qreal viewportHeight, int rowCount, const GraphStyle &style);
int maximumLane(const QVector<Commit> &commits);

} // namespace GitNaga::graph
