#include "graph_geometry.hpp"
#include "graph_palette.hpp"

#include <QTest>

using namespace GitNaga;
using namespace GitNaga::graph;

namespace {

Commit makeCommit(const QString &oid, int lane, const QStringList &parents = {})
{
    Commit commit;
    commit.oid = oid;
    commit.lane = lane;
    commit.parents = parents;
    return commit;
}

} // namespace

class GraphGeometryTest final : public QObject
{
    Q_OBJECT

private slots:
    void lanePositionsAreSpaced()
    {
        const GraphStyle style;
        QCOMPARE(laneX(0, style), style.leftPadding);
        QCOMPARE(laneX(1, style), style.leftPadding + style.laneSpacing);
        QVERIFY(laneX(3, style) > laneX(2, style));
        QCOMPARE(laneAreaWidth(0, style), style.leftPadding + style.laneSpacing);
        QVERIFY(laneAreaWidth(4, style) > laneAreaWidth(1, style));
    }

    void rowCenterTracksContentOffset()
    {
        const GraphStyle style;
        QCOMPARE(rowCenterY(0, 0.0, style), style.rowHeight / 2.0);
        QCOMPARE(rowCenterY(2, 0.0, style), style.rowHeight * 2.0 + style.rowHeight / 2.0);
        QCOMPARE(rowCenterY(0, style.rowHeight, style), -style.rowHeight / 2.0);
    }

    void nodeSitsOnItsLane()
    {
        const GraphStyle style;
        const auto commit = makeCommit(QStringLiteral("a"), 2);
        const auto node = nodeFor(commit, 3, 0.0, style);
        QCOMPARE(node.center.x(), laneX(2, style));
        QCOMPARE(node.center.y(), rowCenterY(3, 0.0, style));
        QVERIFY(node.radius > 0.0);
    }

    void sameLaneEdgeIsStraight()
    {
        const GraphStyle style;
        Commit commit = makeCommit(QStringLiteral("a"), 1);
        commit.segments.append({ 1, 1 });
        const auto edges = edgesFor(commit, 0, 0.0, style, 8);
        QCOMPARE(edges.size(), 1);
        QCOMPARE(edges.first().path.size(), 9);
        for (const auto &point : edges.first().path)
            QCOMPARE(point.x(), laneX(1, style));
        QCOMPARE(edges.first().path.first().y(), rowCenterY(0, 0.0, style));
        QCOMPARE(edges.first().path.last().y(), rowCenterY(1, 0.0, style));
    }

    void crossLaneEdgeSpansBothLanes()
    {
        const GraphStyle style;
        Commit commit = makeCommit(QStringLiteral("a"), 0);
        commit.segments.append({ 0, 2 });
        const auto edges = edgesFor(commit, 0, 0.0, style, 10);
        QCOMPARE(edges.size(), 1);
        const auto &path = edges.first().path;
        QCOMPARE(path.size(), 11);
        QCOMPARE(path.first().x(), laneX(0, style));
        QCOMPARE(path.last().x(), laneX(2, style));
        QCOMPARE(path.first().y(), rowCenterY(0, 0.0, style));
        QCOMPARE(path.last().y(), rowCenterY(1, 0.0, style));
    }

    void maximumLaneFindsWidestRow()
    {
        QVector<Commit> commits;
        commits.append(makeCommit(QStringLiteral("a"), 0));
        commits.append(makeCommit(QStringLiteral("b"), 4));
        commits.append(makeCommit(QStringLiteral("c"), 2));
        QCOMPARE(maximumLane(commits), 4);
        QCOMPARE(maximumLane({}), 0);
    }

    void visibleRangeCoversViewport()
    {
        const GraphStyle style;
        QCOMPARE(firstVisibleRow(0.0, 400.0, style), 0);
        QCOMPARE(lastVisibleRow(0.0, style.rowHeight * 5.0, 100, style), 6);
        QCOMPARE(lastVisibleRow(0.0, style.rowHeight * 5.0, 3, style), 2);
        QCOMPARE(lastVisibleRow(0.0, 400.0, 0, style), -1);
    }

    void paletteWrapsAndStaysDistinct()
    {
        QVERIFY(laneColor(0).isValid());
        QCOMPARE(laneColor(0), laneColor(static_cast<int>(palette().size())));
        QVERIFY(laneColor(0) != laneColor(1));
        QVERIFY(laneColor(-1).isValid());
        QVERIFY(lighten(laneColor(0), 0.5).lightnessF() > laneColor(0).lightnessF());
        QVERIFY(darken(laneColor(0), 0.5).lightnessF() < laneColor(0).lightnessF());
        QCOMPARE(withAlpha(laneColor(0), 12).alpha(), 12);
    }
};

QTEST_GUILESS_MAIN(GraphGeometryTest)

#include "graph_geometry_test.moc"
