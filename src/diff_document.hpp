#pragma once

#include "domain.hpp"
#include "intraline_diff.hpp"
#include "syntax_highlight.hpp"

namespace GitNaga {

// How a file diff is asked for; passed through to git.
struct DiffOptions {
    bool ignoreWhitespace = false;
    int contextLines = 3; // negative: the whole file
    friend bool operator==(const DiffOptions &, const DiffOptions &) = default;
};

// What colours one displayed line: syntax spans and the intra-line changes,
// both over the row's tab-expanded `content`.
struct LineStyle {
    QVector<SyntaxSpan> syntax;
    QVector<TextRange> changed;
};

// The facts git states in a patch header, before the first hunk.
struct DiffFileMeta {
    QString path;
    QString oldPath;
    QString oldMode;
    QString newMode;
    QString oldIndex;
    QString newIndex;
    int similarity = -1;
    bool added = false;
    bool deleted = false;
    bool renamed = false;
    bool copied = false;
    bool binary = false;
    int additions = 0;
    int deletions = 0;
};

struct DiffRow {
    DiffLine::Kind kind = DiffLine::Kind::Context;
    QString text;    // the patch line as git wrote it, marker included
    QString content; // what is shown: marker dropped, tabs expanded
    int oldLine = 0;
    int newLine = 0;
    int hunk = -1;   // index of the hunk this row belongs to; -1 for notices
    int pair = -1;   // the deletion/addition this row replaced or was replaced by
    LineStyle style;
    int source = -1; // index of the patch line this row shows, for staging
};

// One side-by-side row: indexes into DiffDocument::rows, -1 for a blank
// side. A hunk header or notice spans both sides (left == right).
struct SplitRow {
    int hunk = -1;
    int left = -1;
    int right = -1;
    friend bool operator==(const SplitRow &, const SplitRow &) = default;
};

struct DiffDocument {
    DiffFileMeta meta;
    QVector<DiffRow> rows;
    QVector<SplitRow> split;
    int hunkCount = 0;
    int maxColumns = 0;
    int maxLineNumber = 0;
};

DiffFileMeta parseDiffMeta(const QVector<DiffLine> &lines);
DiffDocument buildDiffDocument(const QVector<DiffLine> &lines, const QString &path);

namespace detail {

QString expandTabs(QStringView text, int width = 4);
// Pairs each run of deletions with the additions right after it (marking
// intra-line changes on both), and lays the rows out side by side.
QVector<SplitRow> pairRows(QVector<DiffRow> &rows);

} // namespace detail

} // namespace GitNaga
