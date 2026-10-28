#include "conflict_parser.hpp"

#include <QStringList>

namespace GitNaga::conflicts {

namespace {

constexpr qsizetype minimumMarker = 7;

// Lines with their terminators, so nothing about the file's endings is lost.
QStringList splitLines(const QString &text)
{
    QStringList lines;
    qsizetype start = 0;
    while (start < text.size()) {
        const auto end = text.indexOf(QLatin1Char('\n'), start);
        if (end < 0) {
            lines.append(text.mid(start));
            break;
        }
        lines.append(text.mid(start, end - start + 1));
        start = end + 1;
    }
    return lines;
}

QString stripEnding(QString line)
{
    while (line.endsWith(QLatin1Char('\n')) || line.endsWith(QLatin1Char('\r')))
        line.chop(1);
    return line;
}

// The run of `marker` characters a line starts with, when it is a marker line:
// at least seven of them, then the end of the line or a space and a label.
qsizetype markerRun(const QString &line, QChar marker)
{
    const auto bare = stripEnding(line);
    qsizetype run = 0;
    while (run < bare.size() && bare.at(run) == marker)
        ++run;
    if (run < minimumMarker)
        return 0;
    if (run < bare.size() && bare.at(run) != QLatin1Char(' '))
        return 0;
    return run;
}

QString markerLabel(const QString &line, qsizetype run)
{
    return stripEnding(line).mid(run).trimmed();
}

enum class Section { Text, Ours, Base, Theirs };

} // namespace

int Document::conflictCount() const
{
    int count = 0;
    for (const auto &block : blocks)
        count += block.conflict ? 1 : 0;
    return count;
}

Document parse(const QString &text)
{
    Document document;
    Block plain;
    Block conflict;
    QString raw;
    Section section = Section::Text;
    qsizetype width = 0;

    const auto flushPlain = [&] {
        if (!plain.text.isEmpty())
            document.blocks.append(plain);
        plain = {};
    };

    for (const auto &line : splitLines(text)) {
        if (section == Section::Text) {
            width = markerRun(line, QLatin1Char('<'));
            if (width == 0) {
                plain.text += line;
                continue;
            }
            flushPlain();
            conflict = {};
            conflict.conflict = true;
            conflict.oursLabel = markerLabel(line, width);
            raw = line;
            section = Section::Ours;
            continue;
        }
        raw += line;
        // A closing marker must match the opening one's width: a longer or
        // shorter run is content (a nested file of conflict examples, say).
        if (section != Section::Theirs && markerRun(line, QLatin1Char('|')) == width) {
            conflict.hasBase = true;
            conflict.baseLabel = markerLabel(line, width);
            section = Section::Base;
        } else if (section != Section::Theirs && markerRun(line, QLatin1Char('=')) == width
                   && stripEnding(line).size() == width) {
            section = Section::Theirs;
        } else if (section == Section::Theirs && markerRun(line, QLatin1Char('>')) == width) {
            conflict.theirsLabel = markerLabel(line, width);
            conflict.raw = raw;
            document.blocks.append(conflict);
            section = Section::Text;
        } else if (section == Section::Ours) {
            conflict.ours += line;
        } else if (section == Section::Base) {
            conflict.base += line;
        } else {
            conflict.theirs += line;
        }
    }
    if (section != Section::Text)
        plain.text += raw;
    flushPlain();
    return document;
}

QString resolvedText(const Block &block, Choice choice)
{
    switch (choice) {
    case Choice::Ours: return block.ours;
    case Choice::Theirs: return block.theirs;
    case Choice::OursThenTheirs: return block.ours + block.theirs;
    case Choice::TheirsThenOurs: return block.theirs + block.ours;
    case Choice::Unresolved: break;
    }
    return block.raw;
}

QString resolve(const Document &document, const QVector<Choice> &choices)
{
    QString text;
    qsizetype conflictIndex = 0;
    for (const auto &block : document.blocks) {
        if (!block.conflict) {
            text += block.text;
            continue;
        }
        const auto choice = conflictIndex < choices.size() ? choices.at(conflictIndex) : Choice::Unresolved;
        ++conflictIndex;
        text += resolvedText(block, choice);
    }
    return text;
}

} // namespace GitNaga::conflicts
