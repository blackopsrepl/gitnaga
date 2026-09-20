#include "git_parse.hpp"

#include <QRegularExpression>

#include <algorithm>

namespace GitNaga::detail {

QString decode(const QByteArray &value)
{
    return QString::fromUtf8(value);
}

void assignGraphLayout(QVector<Commit> &commits)
{
    QStringList lanes;
    for (auto &commit : commits) {
        int lane = static_cast<int>(lanes.indexOf(commit.oid));
        if (lane < 0) {
            lane = static_cast<int>(lanes.size());
            lanes.append(commit.oid);
        }

        const auto before = lanes;
        lanes.removeAt(lane);

        int insertion = lane;
        for (const auto &parent : commit.parents) {
            if (!lanes.contains(parent)) {
                lanes.insert(std::min(insertion, static_cast<int>(lanes.size())), parent);
                ++insertion;
            }
        }

        commit.lane = lane;
        commit.laneCount = static_cast<int>(std::max(before.size(), lanes.size()));

        for (int from = 0; from < before.size(); ++from) {
            const auto &oid = before.at(from);
            if (oid == commit.oid)
                continue;
            const int to = static_cast<int>(lanes.indexOf(oid));
            if (to >= 0)
                commit.segments.append({ from, to });
        }
        for (const auto &parent : commit.parents) {
            const int to = static_cast<int>(lanes.indexOf(parent));
            if (to >= 0)
                commit.segments.append({ lane, to });
        }
    }
}

QVector<DiffLine> parseDiff(const QByteArray &data)
{
    QVector<DiffLine> result;
    int oldLine = 0;
    int newLine = 0;
    const QRegularExpression hunkExpression(QStringLiteral(R"(^@@ -(\d+)(?:,\d+)? \+(\d+)(?:,\d+)? @@)"));

    for (const auto &rawLine : data.split('\n')) {
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
