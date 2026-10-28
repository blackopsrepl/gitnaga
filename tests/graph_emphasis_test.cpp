#include "git_parse.hpp"
#include "graph_emphasis.hpp"
#include "graph_geometry.hpp"

#include <QRandomGenerator>
#include <QTest>

using namespace GitNaga;
using namespace GitNaga::graph;

namespace {

// A random history in topological order: every parent sits below its child,
// some rows merge, some are roots, several tips.
QVector<Commit> randomHistory(int size, quint32 seed)
{
    QRandomGenerator random(seed);
    QVector<Commit> commits(size);
    for (int row = 0; row < size; ++row) {
        commits[row].oid = QStringLiteral("c%1").arg(row);
        const int remaining = size - row - 1;
        if (remaining == 0 || random.bounded(40) == 0)
            continue;
        const int parents = random.bounded(5) == 0 ? 2 : 1;
        for (int index = 0; index < parents; ++index) {
            const int parent = row + 1 + random.bounded(std::min(remaining, index == 0 ? 6 : 30));
            const auto oid = QStringLiteral("c%1").arg(parent);
            if (!commits[row].parents.contains(oid))
                commits[row].parents.append(oid);
        }
    }
    detail::assignGraphLayout(commits);
    return commits;
}

void prependWorkInProgress(QVector<Commit> &commits, int headRow)
{
    Commit entry;
    entry.oid = worktreeOid();
    entry.parents = { commits.at(headRow).oid };
    entry.lane = commits.at(headRow).lane;
    entry.colorIndex = commits.at(headRow).colorIndex;
    commits.prepend(entry);
}

QHash<QString, int> indexOf(const QVector<Commit> &commits)
{
    QHash<QString, int> rows;
    for (qsizetype row = 0; row < commits.size(); ++row)
        rows.insert(commits.at(row).oid, static_cast<int>(row));
    return rows;
}

// The bounded emphasis must read exactly like the full-history reference for
// every row it prepared: same dimming, same trace strength on every line.
void compareWindow(const QVector<Commit> &commits, Emphasis &emphasis, int selected, int hovered, int first,
                   int last)
{
    const auto rows = indexOf(commits);
    const auto oids = emphasisOids(commits, rows, selected, hovered);
    const auto spans = emphasisSpans(commits, oids);
    emphasis.setSources(selected, hovered);
    emphasis.prepare(commits, [&rows](const QString &oid) { return rows.value(oid, -1); }, last, 6);
    QSet<int> colors;
    for (const auto &commit : commits)
        colors.insert(commit.colorIndex);
    for (int row = first; row <= std::min(last, static_cast<int>(commits.size()) - 1); ++row) {
        const bool expected = oids.isEmpty() || oids.contains(commits.at(row).oid);
        QVERIFY2(emphasis.contains(commits, row) == expected,
                 qPrintable(QStringLiteral("row %1 sel %2 hov %3").arg(row).arg(selected).arg(hovered)));
        for (const int color : colors) {
            for (const int at : { row, row + 1 }) {
                QVERIFY2(qFuzzyCompare(1.0 + emphasis.strength(color, at, 6), 1.0 + spanStrength(spans, color, at, 6)),
                         qPrintable(QStringLiteral("color %1 row %2 sel %3 hov %4").arg(color).arg(at).arg(selected).arg(hovered)));
            }
        }
    }
}

} // namespace

class GraphEmphasisTest final : public QObject
{
    Q_OBJECT

private slots:
    void matchesFullAncestryInEveryWindow()
    {
        for (quint32 seed = 1; seed <= 4; ++seed) {
            // Long enough that sweeps stop at their horizon well before the
            // end, so lines continuing past the viewport are exercised.
            auto commits = randomHistory(1500, seed);
            prependWorkInProgress(commits, 3);
            QRandomGenerator random(seed * 7);
            Emphasis emphasis;
            for (int trial = 0; trial < 40; ++trial) {
                const int selected = random.bounded(10) == 0 ? -1 : random.bounded(1500);
                const int hovered = random.bounded(3) == 0 ? -1 : random.bounded(1500);
                const int first = std::max(0, (selected >= 0 ? selected : 0) + random.bounded(400) - 100);
                compareWindow(commits, emphasis, selected, hovered, first, first + 20);
            }
            // The uncommitted-work row as a source, and while scrolling down.
            for (int first = 0; first < 1480; first += 37)
                compareWindow(commits, emphasis, 0, 2, first, first + 20);
        }
    }

    void continuesAsRowsStreamIn()
    {
        const auto all = randomHistory(500, 42);
        Emphasis emphasis;
        for (const int loaded : { 120, 121, 300, 500 }) {
            const auto prefix = all.mid(0, loaded);
            // Rows appended below never invalidate the sweep.
            compareWindow(prefix, emphasis, 4, 9, 100, 119);
        }
    }

    void inactiveDimsNothing()
    {
        const auto commits = randomHistory(50, 3);
        Emphasis emphasis;
        emphasis.prepare(commits, [](const QString &) { return -1; }, 20, 6);
        QVERIFY(!emphasis.active());
        QVERIFY(emphasis.contains(commits, 10));
        QCOMPARE(emphasis.strength(commits.at(10).colorIndex, 10, 6), 1.0);
    }
};

QTEST_GUILESS_MAIN(GraphEmphasisTest)

#include "graph_emphasis_test.moc"
