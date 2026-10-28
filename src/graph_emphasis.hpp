#pragma once

#include "domain.hpp"

#include <QHash>
#include <QSet>
#include <QVector>

#include <functional>

namespace GitNaga::graph {

// The ancestry of one row, discovered only as far down as someone looks.
// History is in topological order, so every ancestor sits below its
// descendants: sweeping rows top-down with the set of parents still owed
// finds each ancestor exactly when it is reached. Stopping at the viewport
// makes a hover cost the rows on screen, not the history underneath.
//
// A branch line (one colour) is a first-parent chain, so the ancestors on a
// line are always a suffix of it: the first ancestor row per colour is the
// whole membership answer for that line.
class AncestrySweep
{
public:
    void reset(int source);
    int source() const { return m_source; }
    // Sweeps rows until `horizon` (inclusive) or the last loaded row.
    void advance(const QVector<Commit> &commits, int horizon);
    // Last row swept; -1 before the first advance.
    int reached() const { return m_next - 1; }

    // Colour -> first ancestor row of that colour, counting every ancestor.
    const QHash<int, int> &firstRow() const { return m_first; }
    // Colour -> first ancestor row that is a genuine member of the line. Only
    // the uncommitted-work row borrows a line it is not part of, so the two
    // maps differ exactly there.
    const QHash<int, int> &memberFrom() const { return m_member; }
    // Colour -> last ancestor row swept.
    const QHash<int, int> &lastRow() const { return m_last; }

private:
    int m_source = -1;
    int m_next = 0;
    QSet<QString> m_owed;
    QHash<int, int> m_first;
    QHash<int, int> m_member;
    QHash<int, int> m_last;
};

// The selection's and the hover's ancestry together, answered for the rows
// in view. The selection's sweep is kept across hovers and scrolls; a hover
// only sweeps from its own row to the bottom of the viewport.
class Emphasis
{
public:
    using RowOf = std::function<int(const QString &)>;

    void setSources(int selected, int hovered);
    // The rows changed under the sweeps (reset, removal, a row on top).
    void invalidate();
    bool active() const { return m_selected.source() >= 0 || m_hovered.source() >= 0; }

    // Makes every row up to `lastRow` (plus the fade) answerable.
    void prepare(const QVector<Commit> &commits, const RowOf &rowOf, int lastRow, int fade);

    bool contains(const QVector<Commit> &commits, int row) const;
    // Trace strength of a line at a row, identical to spanStrength() over the
    // full-history spans for every row `prepare` covered.
    qreal strength(int colorIndex, int row, int fade) const;

private:
    void merge(const QVector<Commit> &commits, const RowOf &rowOf, const AncestrySweep &sweep);

    AncestrySweep m_selected;
    AncestrySweep m_hovered;
    // Colour -> [first, last] traced row, as emphasisSpans() would report it
    // for the prepared rows.
    QHash<int, QPair<int, int>> m_spans;
    qsizetype m_mergedRows = 0;
};

} // namespace GitNaga::graph
