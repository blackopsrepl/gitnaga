#include "git_parse.hpp"
#include "graph_geometry.hpp"
#include "graph_palette.hpp"

#include <QTest>

#include <cmath>

namespace {

// CIELab distance, so the palette contract is expressed in perceptual units
// rather than in raw channel deltas.
double labDistance(const QColor &first, const QColor &second)
{
    const auto toLab = [](const QColor &color) {
        const auto linear = [](double channel) {
            return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
        };
        const double r = linear(color.redF());
        const double g = linear(color.greenF());
        const double b = linear(color.blueF());
        const double x = (r * 0.4124564 + g * 0.3575761 + b * 0.1804375) / 0.95047;
        const double y = r * 0.2126729 + g * 0.7151522 + b * 0.0721750;
        const double z = (r * 0.0193339 + g * 0.1191920 + b * 0.9503041) / 1.08883;
        const auto f = [](double t) { return t > 0.008856 ? std::cbrt(t) : 7.787 * t + 16.0 / 116.0; };
        return std::array<double, 3>{ 116.0 * f(y) - 16.0, 500.0 * (f(x) - f(y)), 200.0 * (f(y) - f(z)) };
    };
    const auto a = toLab(first);
    const auto b = toLab(second);
    return std::sqrt(std::pow(a[0] - b[0], 2) + std::pow(a[1] - b[1], 2) + std::pow(a[2] - b[2], 2));
}

double contrastOnBackground(const QColor &color)
{
    const auto luminance = [](const QColor &c) {
        const auto linear = [](double channel) {
            return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * linear(c.redF()) + 0.7152 * linear(c.greenF()) + 0.0722 * linear(c.blueF());
    };
    const QColor background(QStringLiteral("#080b12"));
    return (luminance(color) + 0.05) / (luminance(background) + 0.05);
}

int hueOf(const QColor &color)
{
    return color.hsvHue();
}

} // namespace

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

    void paletteStaysDistinguishable()
    {
        // Regression: the earlier palette held two greens at dE 20 and two
        // violets at dE 21, so distinct branches read as the same colour.
        const auto &colors = palette();
        double closest = 1e9;
        for (std::size_t i = 0; i < colors.size(); ++i) {
            for (std::size_t j = i + 1; j < colors.size(); ++j)
                closest = std::min(closest, labDistance(colors.at(i), colors.at(j)));
        }
        QVERIFY2(closest >= 24.0, qPrintable(QStringLiteral("closest pair dE %1").arg(closest)));

        // Adjacent indices are the lanes a reader compares side by side.
        double closestNeighbour = 1e9;
        for (std::size_t i = 0; i + 1 < colors.size(); ++i)
            closestNeighbour = std::min(closestNeighbour, labDistance(colors.at(i), colors.at(i + 1)));
        QVERIFY2(closestNeighbour >= 80.0,
                 qPrintable(QStringLiteral("closest neighbours dE %1").arg(closestNeighbour)));

        // Thin strokes on the near-black window must stay legible, the first
        // branch must lead with green (red is reserved for destructive
        // actions), the set must lead with green overall, and it may not carry
        // more than one violet entry.
        int greens = 0;
        int violets = 0;
        for (const auto &color : colors) {
            QVERIFY2(contrastOnBackground(color) >= 4.0,
                     qPrintable(QStringLiteral("%1 contrast %2").arg(color.name(),
                                                                    QString::number(contrastOnBackground(color)))));
            const int hue = hueOf(color);
            if (hue >= 70 && hue <= 185)
                ++greens;
            if (hue >= 250 && hue <= 300)
                ++violets;
        }
        QVERIFY2(greens >= 3, qPrintable(QStringLiteral("green entries %1").arg(greens)));
        QVERIFY(violets <= 1);
        const int firstHue = hueOf(colors.front());
        QVERIFY2(firstHue >= 70 && firstHue <= 185,
                 qPrintable(QStringLiteral("first entry hue %1 is not green").arg(firstHue)));
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
