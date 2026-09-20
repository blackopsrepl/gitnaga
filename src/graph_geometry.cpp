#include "graph_geometry.hpp"

#include <algorithm>
#include <cmath>

namespace GitNaga::graph {

qreal laneX(int lane, const GraphStyle &style)
{
    return style.leftPadding + static_cast<qreal>(lane) * style.laneSpacing;
}

qreal rowCenterY(int row, qreal contentY, const GraphStyle &style)
{
    return static_cast<qreal>(row) * style.rowHeight - contentY + style.rowHeight / 2.0;
}

qreal laneAreaWidth(int maximumLane, const GraphStyle &style)
{
    const int lanes = std::max(0, maximumLane) + 1;
    return style.leftPadding + static_cast<qreal>(lanes) * style.laneSpacing;
}

Node nodeFor(const Commit &commit, int row, qreal contentY, const GraphStyle &style)
{
    return Node{ QPointF(laneX(commit.lane, style), rowCenterY(row, contentY, style)), 7.0, commit.lane };
}

QVector<Edge> edgesFor(const Commit &commit, int row, qreal contentY, const GraphStyle &style, int samples)
{
    QVector<Edge> edges;
    if (samples < 1)
        samples = 1;

    const qreal y0 = rowCenterY(row, contentY, style);
    const qreal y1 = rowCenterY(row + 1, contentY, style);

    for (const auto &segment : commit.segments) {
        const qreal x0 = laneX(segment.fromLane, style);
        const qreal x1 = laneX(segment.toLane, style);

        Edge edge;
        edge.fromLane = segment.fromLane;
        edge.toLane = segment.toLane;
        edge.path.reserve(samples + 1);

        if (qFuzzyCompare(x0, x1)) {
            for (int index = 0; index <= samples; ++index) {
                const qreal t = static_cast<qreal>(index) / static_cast<qreal>(samples);
                edge.path.append(QPointF(x0, y0 + (y1 - y0) * t));
            }
        } else {
            const qreal control = y0 + (y1 - y0) * 0.5;
            for (int index = 0; index <= samples; ++index) {
                const qreal t = static_cast<qreal>(index) / static_cast<qreal>(samples);
                const qreal inverse = 1.0 - t;
                const qreal x = inverse * inverse * inverse * x0 + 3.0 * inverse * inverse * t * x0
                                + 3.0 * inverse * t * t * x1 + t * t * t * x1;
                const qreal y = inverse * inverse * inverse * y0 + 3.0 * inverse * inverse * t * control
                                + 3.0 * inverse * t * t * control + t * t * t * y1;
                edge.path.append(QPointF(x, y));
            }
        }
        edges.append(std::move(edge));
    }
    return edges;
}

int firstVisibleRow(qreal contentY, qreal viewportHeight, const GraphStyle &style)
{
    Q_UNUSED(viewportHeight);
    if (style.rowHeight <= 0.0)
        return 0;
    return std::max(0, static_cast<int>(std::floor(contentY / style.rowHeight)) - 1);
}

int lastVisibleRow(qreal contentY, qreal viewportHeight, int rowCount, const GraphStyle &style)
{
    if (rowCount <= 0 || style.rowHeight <= 0.0)
        return -1;
    const int last = static_cast<int>(std::ceil((contentY + viewportHeight) / style.rowHeight)) + 1;
    return std::min(rowCount - 1, last);
}

int maximumLane(const QVector<Commit> &commits)
{
    int maximum = 0;
    for (const auto &commit : commits)
        maximum = std::max(maximum, commit.lane);
    return maximum;
}

} // namespace GitNaga::graph
