#include "graph_emphasis.hpp"

#include "graph_geometry.hpp"

#include <algorithm>

namespace GitNaga::graph {

void AncestrySweep::reset(int source)
{
    m_source = source;
    m_next = std::max(source, 0);
    m_owed.clear();
    m_first.clear();
    m_member.clear();
    m_last.clear();
}

void AncestrySweep::advance(const QVector<Commit> &commits, int horizon)
{
    if (m_source < 0 || m_source >= commits.size())
        return;
    const int end = std::min(horizon, static_cast<int>(commits.size()) - 1);
    for (; m_next <= end; ++m_next) {
        const auto &commit = commits.at(m_next);
        if (m_next != m_source && !m_owed.remove(commit.oid))
            continue;
        const int color = commit.colorIndex;
        if (!m_first.contains(color))
            m_first.insert(color, m_next);
        if (!m_member.contains(color) && commit.oid != worktreeOid())
            m_member.insert(color, m_next);
        m_last.insert(color, m_next);
        for (const auto &parent : commit.parents)
            m_owed.insert(parent);
    }
}

void Emphasis::setSources(int selected, int hovered)
{
    if (hovered == selected)
        hovered = -1;
    const bool changed = selected != m_selected.source() || hovered != m_hovered.source();
    if (selected != m_selected.source())
        m_selected.reset(selected);
    if (hovered != m_hovered.source())
        m_hovered.reset(hovered);
    if (changed)
        m_spans.clear();
}

void Emphasis::invalidate()
{
    m_selected.reset(m_selected.source());
    m_hovered.reset(m_hovered.source());
    m_spans.clear();
}

void Emphasis::prepare(const QVector<Commit> &commits, const RowOf &rowOf, int lastRow, int fade)
{
    if (!active() || commits.isEmpty())
        return;
    // A little beyond what the fade needs, so slow scrolling sweeps in steps
    // instead of on every frame.
    const int needed = std::min(lastRow + fade + 1, static_cast<int>(commits.size()) - 1);
    const int horizon = needed + 128;
    bool advanced = false;
    for (auto *sweep : { &m_selected, &m_hovered }) {
        if (sweep->source() >= 0 && sweep->reached() < needed) {
            sweep->advance(commits, horizon);
            advanced = true;
        }
    }
    // Rows streaming in can extend a line that ended at the last loaded row.
    if (!advanced && !m_spans.isEmpty() && m_mergedRows == commits.size())
        return;
    m_mergedRows = commits.size();
    m_spans.clear();
    merge(commits, rowOf, m_selected);
    merge(commits, rowOf, m_hovered);
}

void Emphasis::merge(const QVector<Commit> &commits, const RowOf &rowOf, const AncestrySweep &sweep)
{
    if (sweep.source() < 0)
        return;
    for (auto it = sweep.firstRow().constBegin(); it != sweep.firstRow().constEnd(); ++it) {
        const int color = it.key();
        int last = sweep.lastRow().value(color, it.value());
        // The sweep stopped at its horizon, but a line whose next commit is
        // already loaded keeps going: its first parent is an ancestor too.
        // Any row past the horizon reads the same for the rows in view.
        const auto &tail = commits.at(last);
        if (!tail.parents.isEmpty()) {
            const int next = rowOf(tail.parents.first());
            if (next > last && commits.at(next).colorIndex == color)
                last = next;
        }
        auto found = m_spans.find(color);
        if (found == m_spans.end()) {
            m_spans.insert(color, { it.value(), last });
        } else {
            found->first = std::min(found->first, it.value());
            found->second = std::max(found->second, last);
        }
    }
}

bool Emphasis::contains(const QVector<Commit> &commits, int row) const
{
    if (!active())
        return true;
    if (row == m_selected.source() || row == m_hovered.source())
        return true;
    if (row < 0 || row >= commits.size() || commits.at(row).oid == worktreeOid())
        return false;
    const int color = commits.at(row).colorIndex;
    for (const auto *sweep : { &m_selected, &m_hovered }) {
        if (sweep->source() < 0)
            continue;
        const auto found = sweep->memberFrom().constFind(color);
        if (found != sweep->memberFrom().constEnd() && *found <= row)
            return true;
    }
    return false;
}

qreal Emphasis::strength(int colorIndex, int row, int fade) const
{
    if (!active())
        return 1.0;
    // A source below the viewport traces nothing in it yet; an empty span
    // table here means "nothing traced", not "nothing dimmed".
    if (m_spans.isEmpty())
        return 0.0;
    return spanStrength(m_spans, colorIndex, row, fade);
}

} // namespace GitNaga::graph
