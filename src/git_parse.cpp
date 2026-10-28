#include "git_parse.hpp"

#include <QRegularExpression>

#include <algorithm>

namespace GitNaga::detail {

QString decode(const QByteArray &value)
{
    return QString::fromUtf8(value);
}

void GraphLayout::place(Commit &commit)
{
    // Each lane entry is one line of history waiting for its commit. Colours
    // ride alongside the entries so a line keeps its colour as lanes shift:
    // only a genuinely new line gets a fresh colour index.
    auto &lanes = m_lanes;
    auto &laneColors = m_laneColors;
    int lane = static_cast<int>(lanes.indexOf(commit.oid));
    if (lane < 0) {
        lane = static_cast<int>(lanes.size());
        lanes.append(commit.oid);
        laneColors.append(m_nextColor++);
    }

    const auto before = lanes;
    const auto beforeColors = laneColors;
    const int color = laneColors.at(lane);
    lanes.removeAt(lane);
    laneColors.removeAt(lane);

    int insertion = lane;
    for (qsizetype parent = 0; parent < commit.parents.size(); ++parent) {
        const auto &oid = commit.parents.at(parent);
        if (lanes.contains(oid))
            continue;
        // The first parent continues this line and its colour; additional
        // parents open new lines with colours of their own.
        const int parentColor = parent == 0 ? color : m_nextColor++;
        lanes.insert(std::min(insertion, static_cast<int>(lanes.size())), oid);
        laneColors.insert(std::min(insertion, static_cast<int>(laneColors.size())), parentColor);
        ++insertion;
    }

    commit.lane = lane;
    commit.colorIndex = color;
    commit.laneCount = static_cast<int>(std::max(before.size(), lanes.size()));
    commit.segments.clear();

    for (int from = 0; from < before.size(); ++from) {
        const auto &oid = before.at(from);
        if (oid == commit.oid)
            continue;
        const int to = static_cast<int>(lanes.indexOf(oid));
        if (to >= 0)
            commit.segments.append({ from, to, beforeColors.at(from), laneColors.at(to) });
    }
    for (const auto &parent : commit.parents) {
        const int to = static_cast<int>(lanes.indexOf(parent));
        if (to >= 0)
            commit.segments.append({ lane, to, color, laneColors.at(to) });
    }
}

void assignGraphLayout(QVector<Commit> &commits)
{
    GraphLayout layout;
    for (auto &commit : commits)
        layout.place(commit);
}

QVector<DiffLine> parseDiff(const QByteArray &data)
{
    QVector<DiffLine> result;
    int oldLine = 0;
    int newLine = 0;
    const QRegularExpression hunkExpression(QStringLiteral(R"(^@@ -(\d+)(?:,\d+)? \+(\d+)(?:,\d+)? @@)"));

    auto rawLines = data.split('\n');
    // Git ends its output with a newline; the empty piece after it is not a
    // line of the file.
    if (!rawLines.isEmpty() && rawLines.last().isEmpty())
        rawLines.removeLast();
    for (const auto &rawLine : rawLines) {
        const auto text = decode(rawLine);
        DiffLine line;
        line.text = text;

        if (text.startsWith(QStringLiteral("@@"))) {
            line.kind = DiffLine::Kind::Hunk;
            const auto match = hunkExpression.match(text);
            if (match.hasMatch()) {
                oldLine = match.captured(1).toInt();
                newLine = match.captured(2).toInt();
            }
        } else if (text.startsWith(QStringLiteral("diff --git"))
                   || text.startsWith(QStringLiteral("index "))
                   || text.startsWith(QStringLiteral("--- "))
                   || text.startsWith(QStringLiteral("+++ "))) {
            line.kind = DiffLine::Kind::Header;
        } else if (text.startsWith(QLatin1Char('+'))) {
            line.kind = DiffLine::Kind::Addition;
            line.newLine = newLine++;
        } else if (text.startsWith(QLatin1Char('-'))) {
            line.kind = DiffLine::Kind::Deletion;
            line.oldLine = oldLine++;
        } else {
            line.kind = DiffLine::Kind::Context;
            if (!text.startsWith(QLatin1Char('\\'))) {
                line.oldLine = oldLine++;
                line.newLine = newLine++;
            }
        }
        result.append(std::move(line));
    }
    return result;
}

} // namespace GitNaga::detail
