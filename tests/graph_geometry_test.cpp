#include "git_parse.hpp"
#include "graph_geometry.hpp"

#include <QTest>

#include <cmath>

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

    void emphasisKeepsSelectionWhileHoveringElsewhere()
    {
        // Two divergent branches: selecting one tip and hovering the other
        // must keep both ancestries emphasised. The regression this guards is
        // hover replacing the selection, which dimmed the selected node.
        QVector<Commit> commits;
        commits.append(makeCommit(QStringLiteral("main-tip"), 0, { QStringLiteral("base") }));
        commits.append(makeCommit(QStringLiteral("feature-tip"), 1, { QStringLiteral("base") }));
        commits.append(makeCommit(QStringLiteral("base"), 0));

        QHash<QString, int> rowByOid;
        for (qsizetype row = 0; row < commits.size(); ++row)
            rowByOid.insert(commits.at(row).oid, static_cast<int>(row));
        const auto rowOf = [&rowByOid](const QString &oid) { return rowByOid.value(oid, -1); };

        // Nothing active: nothing is emphasised, so nothing is dimmed.
        QVERIFY(emphasisOids(commits, rowByOid, -1, -1).isEmpty());

        // Selection alone traces its own line.
        const auto selectedOnly = emphasisOids(commits, rowByOid, rowOf(QStringLiteral("main-tip")), -1);
        QCOMPARE(selectedOnly, QSet<QString>({ QStringLiteral("main-tip"), QStringLiteral("base") }));

        // Hovering the sibling branch adds its line and keeps the selected one.
        const auto both = emphasisOids(commits, rowByOid, rowOf(QStringLiteral("main-tip")),
                                       rowOf(QStringLiteral("feature-tip")));
        QCOMPARE(both, QSet<QString>({ QStringLiteral("main-tip"), QStringLiteral("feature-tip"),
                                       QStringLiteral("base") }));

        // Hovering alone still traces, which is the reason hover exists.
        const auto hoverOnly = emphasisOids(commits, rowByOid, -1, rowOf(QStringLiteral("feature-tip")));
        QCOMPARE(hoverOnly, QSet<QString>({ QStringLiteral("feature-tip"), QStringLiteral("base") }));
    }

    void branchLinesKeepTheirColour()
    {
        // main-tip and feature-tip diverge from base; the colour must follow
        // each line for its whole run instead of changing when lanes shift.
        QVector<Commit> commits;
        commits.append(makeCommit(QStringLiteral("main-tip"), 0, { QStringLiteral("main-1") }));
        commits.append(makeCommit(QStringLiteral("feature-tip"), 0, { QStringLiteral("feature-1") }));
        commits.append(makeCommit(QStringLiteral("main-1"), 0, { QStringLiteral("base") }));
        commits.append(makeCommit(QStringLiteral("feature-1"), 0, { QStringLiteral("base") }));
        commits.append(makeCommit(QStringLiteral("base"), 0));

        GitNaga::detail::assignGraphLayout(commits);

        const auto colorOf = [&commits](const QString &oid) {
            for (const auto &commit : commits) {
                if (commit.oid == oid)
                    return commit.colorIndex;
            }
            return -1;
        };
        QCOMPARE(colorOf(QStringLiteral("main-tip")), colorOf(QStringLiteral("main-1")));
        QCOMPARE(colorOf(QStringLiteral("feature-tip")), colorOf(QStringLiteral("feature-1")));
        QVERIFY(colorOf(QStringLiteral("main-tip")) != colorOf(QStringLiteral("feature-tip")));

        // Every edge carries the colours of the lines it connects.
        for (const auto &commit : commits) {
            for (const auto &segment : commit.segments) {
                QVERIFY(segment.fromColor >= 0);
                QVERIFY(segment.toColor >= 0);
            }
        }
    }

};

QTEST_GUILESS_MAIN(GraphGeometryTest)

#include "graph_geometry_test.moc"
