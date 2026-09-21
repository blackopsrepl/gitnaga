#include "graph_geometry.hpp"

#include <algorithm>
#include <cmath>

namespace GitNaga::graph {
namespace {

struct Sample {
    QPointF point;
    qreal length = 0.0;
};

void appendQuad(QVector<Sample> &out, const QPointF &p0, const QPointF &control, const QPointF &p1, int steps)
{
    qreal previousLength = out.isEmpty() ? 0.0 : out.last().length;
    for (int step = 1; step <= steps; ++step) {
        const qreal t = static_cast<qreal>(step) / static_cast<qreal>(steps);
        const qreal inverse = 1.0 - t;
        const QPointF point = inverse * inverse * p0 + 2.0 * inverse * t * control + t * t * p1;
        const QPointF delta = point - out.last().point;
        previousLength += std::hypot(delta.x(), delta.y());
        out.append({ point, previousLength });
    }
}

void appendLine(QVector<Sample> &out, const QPointF &to, int steps)
{
    const QPointF from = out.last().point;
    qreal length = out.last().length;
    for (int step = 1; step <= steps; ++step) {
        const qreal t = static_cast<qreal>(step) / static_cast<qreal>(steps);
        const QPointF point = from + (to - from) * t;
        length = out.last().length + std::hypot(point.x() - out.last().point.x(), point.y() - out.last().point.y());
        out.append({ point, length });
    }
}

QPointF evaluate(const QVector<Sample> &samples, qreal target)
{
    if (samples.isEmpty())
        return {};
    const qreal total = samples.last().length;
    if (total <= 0.0)
        return samples.last().point;
    const qreal clamped = std::clamp(target, 0.0, total);
    for (qsizetype index = 1; index < samples.size(); ++index) {
        if (samples.at(index).length >= clamped) {
            const auto &a = samples.at(index - 1);
            const auto &b = samples.at(index);
            const qreal span = b.length - a.length;
            const qreal t = span <= 0.0 ? 0.0 : (clamped - a.length) / span;
            return a.point + (b.point - a.point) * t;
        }
    }
    return samples.last().point;
}

} // namespace

namespace {

void appendAncestry(QSet<QString> &oids, const QVector<Commit> &commits, const QHash<QString, int> &rowByOid,
                    int row)
{
    if (row < 0 || row >= commits.size())
        return;
    QVector<int> pending{ row };
    oids.insert(commits.at(row).oid);
    while (!pending.isEmpty()) {
        const int current = pending.takeLast();
        for (const auto &parent : commits.at(current).parents) {
            const auto found = rowByOid.constFind(parent);
            if (found == rowByOid.constEnd() || oids.contains(parent))
                continue;
            oids.insert(parent);
            pending.append(*found);
        }
    }
}

} // namespace

QSet<QString> emphasisOids(const QVector<Commit> &commits, const QHash<QString, int> &rowByOid,
                           int selectedRow, int hoveredRow)
{
    QSet<QString> oids;
    appendAncestry(oids, commits, rowByOid, selectedRow);
    if (hoveredRow != selectedRow)
        appendAncestry(oids, commits, rowByOid, hoveredRow);
    return oids;
}

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
    if (samples < 2)
        samples = 2;

    const qreal y0 = rowCenterY(row, contentY, style);
    const qreal y1 = rowCenterY(row + 1, contentY, style);

    for (const auto &segment : commit.segments) {
        const qreal x0 = laneX(segment.fromLane, style);
        const qreal x1 = laneX(segment.toLane, style);

        Edge edge;
        edge.fromLane = segment.fromLane;
        edge.toLane = segment.toLane;
        edge.fromColor = segment.fromColor;
        edge.toColor = segment.toColor;

        QVector<Sample> polyline;
        polyline.append({ QPointF(x0, y0), 0.0 });

        if (qFuzzyCompare(x0, x1)) {
            appendLine(polyline, QPointF(x0, y1), samples);
        } else {
            // Elbow routing: hold vertical, rounded turn onto the row boundary,
            // run horizontally, rounded turn, then hold vertical into the parent.
            const qreal boundary = (y0 + y1) / 2.0;
            const qreal dx = x1 - x0;
            const qreal direction = dx > 0.0 ? 1.0 : -1.0;
            const qreal room = std::abs(y1 - y0) * 0.42;
            qreal radius = std::min({ std::abs(dx) / 2.0, room, 12.0 * style.laneSpacing / 22.0 });
            radius = std::max(radius, 0.5);

            const int cornerSteps = std::max(2, samples / 6);

            appendLine(polyline, QPointF(x0, boundary - radius), std::max(1, samples / 5));
            appendQuad(polyline, QPointF(x0, boundary - radius), QPointF(x0, boundary),
                       QPointF(x0 + direction * radius, boundary), cornerSteps);
            appendLine(polyline, QPointF(x1 - direction * radius, boundary), std::max(1, samples / 5));
            appendQuad(polyline, QPointF(x1 - direction * radius, boundary), QPointF(x1, boundary),
                       QPointF(x1, boundary + radius), cornerSteps);
            appendLine(polyline, QPointF(x1, y1), std::max(1, samples / 5));
        }

        edge.path.reserve(samples + 1);
        for (int index = 0; index <= samples; ++index) {
            const qreal t = static_cast<qreal>(index) / static_cast<qreal>(samples);
            edge.path.append(evaluate(polyline, polyline.last().length * t));
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
