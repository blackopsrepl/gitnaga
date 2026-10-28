#include "diff_document.hpp"

#include <QRegularExpression>

#include <algorithm>

namespace GitNaga {

namespace {

bool isHunkHeader(const DiffLine &line)
{
    return line.kind == DiffLine::Kind::Hunk && line.text.startsWith(QStringLiteral("@@"));
}

// "\ No newline at end of file": belongs to the line before it, numbers none.
bool isNote(const DiffRow &row)
{
    return row.kind == DiffLine::Kind::Context && row.oldLine == 0 && row.newLine == 0;
}

QString after(const QString &text, QStringView prefix)
{
    return text.mid(prefix.size()).trimmed();
}

void readHeaderLine(const QString &text, DiffFileMeta &meta)
{
    static const QRegularExpression index(QStringLiteral(R"(^index ([0-9a-f]+)\.\.([0-9a-f]+)(?: (\d+))?)"));
    if (text.startsWith(QStringLiteral("old mode ")))
        meta.oldMode = after(text, u"old mode ");
    else if (text.startsWith(QStringLiteral("new mode ")))
        meta.newMode = after(text, u"new mode ");
    else if (text.startsWith(QStringLiteral("new file mode "))) {
        meta.added = true;
        meta.newMode = after(text, u"new file mode ");
    } else if (text.startsWith(QStringLiteral("deleted file mode "))) {
        meta.deleted = true;
        meta.oldMode = after(text, u"deleted file mode ");
    } else if (text.startsWith(QStringLiteral("similarity index ")))
        meta.similarity = after(text, u"similarity index ").chopped(1).toInt();
    else if (text.startsWith(QStringLiteral("rename from "))) {
        meta.renamed = true;
        meta.oldPath = after(text, u"rename from ");
    } else if (text.startsWith(QStringLiteral("copy from "))) {
        meta.copied = true;
        meta.oldPath = after(text, u"copy from ");
    } else if (text.startsWith(QStringLiteral("Binary files ")) || text == QStringLiteral("GIT binary patch"))
        meta.binary = true;
    else if (const auto match = index.match(text); match.hasMatch()) {
        meta.oldIndex = match.captured(1);
        meta.newIndex = match.captured(2);
        if (!match.captured(3).isEmpty() && meta.oldMode.isEmpty() && meta.newMode.isEmpty()) {
            if (!meta.added)
                meta.oldMode = match.captured(3);
            if (!meta.deleted)
                meta.newMode = match.captured(3);
        }
    }
}

void highlight(DiffDocument &document, const QString &path)
{
    const auto *rules = languageForPath(path);
    if (!rules)
        return;
    HighlightState before;
    HighlightState after;
    for (auto &row : document.rows) {
        switch (row.kind) {
        case DiffLine::Kind::Hunk:
            // A hunk starts mid-file; assume nothing is open.
            before = after = {};
            break;
        case DiffLine::Kind::Deletion: row.style.syntax = highlightLine(rules, row.content, before); break;
        case DiffLine::Kind::Addition: row.style.syntax = highlightLine(rules, row.content, after); break;
        case DiffLine::Kind::Context:
            if (isNote(row))
                break;
            if (before == after) {
                row.style.syntax = highlightLine(rules, row.content, after);
                before = after;
            } else {
                highlightLine(rules, row.content, before);
                row.style.syntax = highlightLine(rules, row.content, after);
            }
            break;
        case DiffLine::Kind::Header: break;
        }
    }
}

} // namespace

DiffFileMeta parseDiffMeta(const QVector<DiffLine> &lines)
{
    DiffFileMeta meta;
    for (const auto &line : lines) {
        if (isHunkHeader(line))
            break;
        readHeaderLine(line.text, meta);
    }
    return meta;
}

DiffDocument buildDiffDocument(const QVector<DiffLine> &lines, const QString &path)
{
    DiffDocument document;
    document.meta = parseDiffMeta(lines);
    document.meta.path = path;
    const auto first = std::find_if(lines.begin(), lines.end(), isHunkHeader);
    // A truncation notice can follow a header-only (binary) patch.
    for (auto line = lines.begin(); line != first; ++line) {
        if (line->kind == DiffLine::Kind::Hunk)
            document.rows.append({ line->kind, line->text, line->text, 0, 0, -1, -1, {},
                                   static_cast<int>(line - lines.begin()) });
    }
    int hunk = -1;
    for (auto line = first; line != lines.end(); ++line) {
        if (line->kind == DiffLine::Kind::Header)
            continue;
        DiffRow row;
        row.source = static_cast<int>(line - lines.begin());
        row.kind = line->kind;
        row.text = line->text;
        row.oldLine = line->oldLine;
        row.newLine = line->newLine;
        if (line->kind == DiffLine::Kind::Hunk) {
            const bool header = isHunkHeader(*line);
            if (header)
                ++hunk;
            row.hunk = header ? hunk : -1;
            row.content = line->text;
        } else {
            row.hunk = hunk;
            const bool marked = !line->text.startsWith(QLatin1Char('\\'));
            row.content = detail::expandTabs(marked ? QStringView(line->text).mid(1) : QStringView(line->text));
            document.maxColumns = std::max(document.maxColumns, static_cast<int>(row.content.size()));
            document.maxLineNumber = std::max({ document.maxLineNumber, row.oldLine, row.newLine });
        }
        document.meta.additions += row.kind == DiffLine::Kind::Addition ? 1 : 0;
        document.meta.deletions += row.kind == DiffLine::Kind::Deletion ? 1 : 0;
        document.rows.append(std::move(row));
    }
    document.hunkCount = hunk + 1;
    highlight(document, path);
    document.split = detail::pairRows(document.rows);
    return document;
}

namespace detail {

QString expandTabs(QStringView text, int width)
{
    if (!text.contains(QLatin1Char('\t')))
        return text.toString();
    QString result;
    result.reserve(text.size() + 8);
    for (const auto character : text) {
        if (character == QLatin1Char('\t'))
            result.append(QString(width - static_cast<int>(result.size() % width), QLatin1Char(' ')));
        else
            result.append(character);
    }
    return result;
}

QString showWhitespace(QStringView text, int width)
{
    QString result;
    result.reserve(text.size() + 8);
    for (const auto character : text) {
        if (character == QLatin1Char('\t')) {
            result.append(QChar(0x2192));
            result.append(QString(width - 1 - static_cast<int>((result.size() - 1) % width), QLatin1Char(' ')));
        } else if (character == QLatin1Char(' ')) {
            result.append(QChar(0x00B7));
        } else {
            result.append(character);
        }
    }
    return result;
}

bool differsOnlyInWhitespace(QStringView before, QStringView after)
{
    if (before == after)
        return false;
    auto squeeze = [](QStringView text) {
        QString kept;
        for (const auto character : text)
            if (!character.isSpace())
                kept.append(character);
        return kept;
    };
    return squeeze(before) == squeeze(after);
}

QVector<SplitRow> pairRows(QVector<DiffRow> &rows)
{
    QVector<SplitRow> split;
    split.reserve(rows.size());
    const auto count = static_cast<int>(rows.size());
    int index = 0;
    while (index < count) {
        const auto kind = rows.at(index).kind;
        if (kind != DiffLine::Kind::Deletion && kind != DiffLine::Kind::Addition) {
            split.append({ rows.at(index).hunk, index, index });
            ++index;
            continue;
        }
        // One change block: deletions, then additions, each possibly followed
        // by its "no newline" note, which stays on its own side.
        QVector<int> removed, added, removedNotes, addedNotes;
        const int hunk = rows.at(index).hunk;
        while (index < count && rows.at(index).kind == DiffLine::Kind::Deletion) {
            removed.append(index++);
            if (index < count && isNote(rows.at(index)))
                removedNotes.append(index++);
        }
        while (index < count && rows.at(index).kind == DiffLine::Kind::Addition) {
            added.append(index++);
            if (index < count && isNote(rows.at(index)))
                addedNotes.append(index++);
        }
        const auto paired = std::min(removed.size(), added.size());
        for (qsizetype n = 0; n < paired; ++n) {
            auto &before = rows[removed.at(n)];
            auto &after = rows[added.at(n)];
            before.pair = added.at(n);
            after.pair = removed.at(n);
            // Tabs against spaces, or trailing blanks, look identical once
            // expanded: show the whitespace itself so the change can be seen.
            // Each marker keeps its column, so syntax spans stay aligned.
            const auto oldText = QStringView(before.text).mid(1);
            const auto newText = QStringView(after.text).mid(1);
            if (differsOnlyInWhitespace(oldText, newText)) {
                before.content = showWhitespace(oldText);
                after.content = showWhitespace(newText);
            }
            auto changes = intralineDiff(before.content, after.content);
            before.style.changed = std::move(changes.removed);
            after.style.changed = std::move(changes.added);
        }
        for (qsizetype n = 0; n < std::max(removed.size(), added.size()); ++n)
            split.append({ hunk, n < removed.size() ? removed.at(n) : -1, n < added.size() ? added.at(n) : -1 });
        for (const auto note : removedNotes)
            split.append({ hunk, note, -1 });
        for (const auto note : addedNotes)
            split.append({ hunk, -1, note });
    }
    return split;
}

} // namespace detail

} // namespace GitNaga
