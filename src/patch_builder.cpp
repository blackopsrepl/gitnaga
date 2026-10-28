#include "patch_builder.hpp"

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <optional>

namespace GitNaga::patch {

namespace {

const QString kNoNewline = QStringLiteral("\\ No newline at end of file");

bool isHunkHeader(const QString &text)
{
    return text.startsWith(QStringLiteral("@@ -"));
}

struct Range {
    int start = 0;
    int count = 0;
};

struct HunkHeader {
    Range before;
    Range after;
    QString section;
};

std::optional<HunkHeader> parseHeader(const QString &text)
{
    static const QRegularExpression expression(
        QStringLiteral(R"(^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@(.*)$)"));
    const auto match = expression.match(text);
    if (!match.hasMatch())
        return std::nullopt;
    const auto count = [&match](int group) {
        return match.captured(group).isEmpty() ? 1 : match.captured(group).toInt();
    };
    return HunkHeader{ { match.captured(1).toInt(), count(2) }, { match.captured(3).toInt(), count(4) },
                       match.captured(5) };
}

// Body rows of the hunk whose header is at `header`: up to the next header.
int hunkEnd(const QVector<DiffLine> &lines, int header)
{
    int row = header + 1;
    while (row < lines.size() && !isHunkHeader(lines.at(row).text))
        ++row;
    return row;
}

QChar marker(const QString &text)
{
    // A blank context line may come through with its leading space trimmed.
    return text.isEmpty() ? QLatin1Char(' ') : text.at(0);
}

bool isChange(const QString &text)
{
    const auto prefix = marker(text);
    return prefix == QLatin1Char('+') || prefix == QLatin1Char('-');
}

struct Hunk {
    QStringList body;
    int before = 0;
    int after = 0;
};

class HunkWriter
{
public:
    HunkWriter(const QVector<DiffLine> &lines, const QSet<int> &selected, Direction direction, int first, int end)
        : m_lines(lines), m_selected(selected), m_forward(direction == Direction::Forward), m_end(end)
    {
        for (int row = first; row < end; ++row)
            writeRow(row);
    }

    Hunk hunk() const { return m_hunk; }

private:
    void writeRow(int row)
    {
        const auto &text = m_lines.at(row).text;
        const auto prefix = marker(text);
        if (prefix == QLatin1Char('\\'))
            return; // written together with the line it belongs to
        const auto body = text.mid(1);
        const bool missingNewline = row + 1 < m_end && m_lines.at(row + 1).text.startsWith(QLatin1Char('\\'));
        if (prefix == QLatin1Char(' ')) {
            emitLine(QLatin1Char(' '), body, missingNewline);
            return;
        }
        // The base side keeps its lines: a forward patch applies onto the
        // old side, a reverse patch onto the new side.
        const bool baseLine = prefix == (m_forward ? QLatin1Char('-') : QLatin1Char('+'));
        if (m_selected.contains(row))
            emitLine(prefix, body, missingNewline);
        else if (baseLine)
            keepAsContext(row, body, missingNewline);
    }

    void keepAsContext(int row, const QString &body, bool missingNewline)
    {
        // A base line that ends the file without a newline cannot stay as
        // plain context when selected lines get appended after it: the
        // line gains a newline on the other side, so it is rewritten as a
        // remove and re-add that differ only in that newline.
        if (!missingNewline || !laterGrowth(row)) {
            emitLine(QLatin1Char(' '), body, missingNewline);
            return;
        }
        emitLine(QLatin1Char('-'), body, m_forward);
        emitLine(QLatin1Char('+'), body, !m_forward);
    }

    bool laterGrowth(int row) const
    {
        const auto grows = m_forward ? QLatin1Char('+') : QLatin1Char('-');
        for (int later = row + 1; later < m_end; ++later)
            if (marker(m_lines.at(later).text) == grows && m_selected.contains(later))
                return true;
        return false;
    }

    void emitLine(QChar prefix, const QString &body, bool missingNewline)
    {
        m_hunk.body.append(prefix + body);
        if (missingNewline)
            m_hunk.body.append(kNoNewline);
        if (prefix != QLatin1Char('+'))
            ++m_hunk.before;
        if (prefix != QLatin1Char('-'))
            ++m_hunk.after;
    }

    const QVector<DiffLine> &m_lines;
    const QSet<int> &m_selected;
    const bool m_forward;
    const int m_end;
    Hunk m_hunk;
};

// The `---`/`+++` token after the marker, e.g. `a/src/x.cpp` or `"a/sp ace"`.
QString pathToken(const QString &line)
{
    return line.mid(4);
}

QString withSide(QString token, QChar side)
{
    const qsizetype at = token.startsWith(QLatin1Char('"')) ? 1 : 0;
    if (token.size() > at + 1 && token.at(at + 1) == QLatin1Char('/'))
        token[at] = side;
    return token;
}

// A partial patch changes content only: the side the patch does not apply
// onto is redirected to the base path, so a partial stage never creates,
// deletes or renames the file behind the user's back.
QStringList partialHeader(const QStringList &header, Direction direction)
{
    QString oldToken;
    QString newToken;
    for (const auto &line : header) {
        if (line.startsWith(QStringLiteral("--- ")))
            oldToken = pathToken(line);
        else if (line.startsWith(QStringLiteral("+++ ")))
            newToken = pathToken(line);
    }
    const bool forward = direction == Direction::Forward;
    const auto base = forward ? oldToken : newToken;
    if (base == QStringLiteral("/dev/null"))
        return header; // creating or deleting from nothing is already partial
    const QStringList dropped = {
        QStringLiteral("index "), QStringLiteral("similarity index"), QStringLiteral("dissimilarity index"),
        QStringLiteral("rename from"), QStringLiteral("rename to"), QStringLiteral("copy from"),
        QStringLiteral("copy to"), forward ? QStringLiteral("deleted file mode") : QStringLiteral("new file mode"),
    };
    QStringList result;
    for (const auto &line : header) {
        if (std::ranges::any_of(dropped, [&line](const QString &prefix) { return line.startsWith(prefix); }))
            continue;
        if (line.startsWith(QStringLiteral("diff --git ")))
            result.append(QStringLiteral("diff --git ") + withSide(base, QLatin1Char('a')) + QLatin1Char(' ')
                          + withSide(base, QLatin1Char('b')));
        else if (line.startsWith(QStringLiteral("--- ")))
            result.append(QStringLiteral("--- ") + withSide(base, QLatin1Char('a')));
        else if (line.startsWith(QStringLiteral("+++ ")))
            result.append(QStringLiteral("+++ ") + withSide(base, QLatin1Char('b')));
        else
            result.append(line);
    }
    return result;
}

// Hunk starts name the first line of the range, or the line before it when
// the range is empty; the side the patch is not applied onto follows the base
// side, shifted by what the earlier hunks of this patch added or removed.
int startFor(int baseStart, int baseCount, int count, int shift)
{
    const int first = (baseCount > 0 ? baseStart : baseStart + 1) + shift;
    return count > 0 ? first : first - 1;
}

} // namespace

QSet<int> hunkChanges(const QVector<DiffLine> &lines, int hunkRow)
{
    QSet<int> rows;
    if (hunkRow < 0 || hunkRow >= lines.size() || !isHunkHeader(lines.at(hunkRow).text))
        return rows;
    const int end = hunkEnd(lines, hunkRow);
    for (int row = hunkRow + 1; row < end; ++row)
        if (isChange(lines.at(row).text))
            rows.insert(row);
    return rows;
}

QSet<int> allChanges(const QVector<DiffLine> &lines)
{
    QSet<int> rows;
    for (int row = 0; row < lines.size(); ++row)
        if (isHunkHeader(lines.at(row).text))
            rows.unite(hunkChanges(lines, row));
    return rows;
}

std::expected<QByteArray, QString> build(const QVector<DiffLine> &lines, const QSet<int> &selected,
                                         Direction direction)
{
    QStringList header;
    int row = 0;
    for (; row < lines.size() && !isHunkHeader(lines.at(row).text); ++row) {
        if (lines.at(row).text.startsWith(QStringLiteral("diff --cc")))
            return std::unexpected(QStringLiteral("A conflicted file is staged as a whole"));
        header.append(lines.at(row).text);
    }
    if (row == lines.size())
        return std::unexpected(QStringLiteral("This change has no lines to stage separately"));

    const auto changes = allChanges(lines);
    QSet<int> chosen = selected;
    chosen.intersect(changes);
    if (chosen.isEmpty())
        return std::unexpected(QStringLiteral("Select changed lines first"));

    const bool forward = direction == Direction::Forward;
    // A rename is a file-level change: staging lines of a renamed file moves
    // content only, even when every line is chosen.
    const bool renamed = std::ranges::any_of(header, [](const QString &line) {
        return line.startsWith(QStringLiteral("rename from")) || line.startsWith(QStringLiteral("copy from"));
    });
    QStringList output = chosen.size() == changes.size() && !renamed ? header : partialHeader(header, direction);
    int shift = 0;
    while (row < lines.size()) {
        const auto &text = lines.at(row).text;
        const auto parsed = parseHeader(text);
        if (!parsed)
            return std::unexpected(QStringLiteral("The diff is incomplete; it cannot be staged by line"));
        const int end = hunkEnd(lines, row);
        const auto mine = hunkChanges(lines, row);
        if (std::ranges::none_of(mine, [&chosen](int change) { return chosen.contains(change); })) {
            row = end;
            continue;
        }
        const auto hunk = HunkWriter(lines, chosen, direction, row + 1, end).hunk();
        const auto base = forward ? parsed->before : parsed->after;
        const int baseCount = forward ? hunk.before : hunk.after;
        const int otherCount = forward ? hunk.after : hunk.before;
        const int otherStart = startFor(base.start, baseCount, otherCount, forward ? shift : -shift);
        const int oldStart = forward ? base.start : otherStart;
        const int newStart = forward ? otherStart : base.start;
        output.append(QStringLiteral("@@ -%1,%2 +%3,%4 @@%5")
                          .arg(oldStart).arg(hunk.before).arg(newStart).arg(hunk.after).arg(parsed->section));
        output.append(hunk.body);
        shift += hunk.after - hunk.before;
        row = end;
    }
    return (output.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8();
}

} // namespace GitNaga::patch
