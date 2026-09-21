#pragma once

#include "domain.hpp"

#include <QHash>
#include <QPointF>
#include <QSet>
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

// Object ids that stay at full strength for the current selection and hover:
// the union of both ancestries, so pointing at one line never erases the
// emphasis of the commit the user actually selected. Empty means "no
// emphasis", which callers read as "dim nothing".
QSet<QString> emphasisOids(const QVector<Commit> &commits, const QHash<QString, int> &rowByOid,
                           int selectedRow, int hoveredRow);

qreal laneX(int lane, const GraphStyle &style);
qreal rowCenterY(int row, qreal contentY, const GraphStyle &style);
qreal laneAreaWidth(int maximumLane, const GraphStyle &style);

Node nodeFor(const Commit &commit, int row, qreal contentY, const GraphStyle &style);
QVector<Edge> edgesFor(const Commit &commit, int row, qreal contentY, const GraphStyle &style, int samples = 18);

int firstVisibleRow(qreal contentY, qreal viewportHeight, const GraphStyle &style);
int lastVisibleRow(qreal contentY, qreal viewportHeight, int rowCount, const GraphStyle &style);
int maximumLane(const QVector<Commit> &commits);

} // namespace GitNaga::graph
