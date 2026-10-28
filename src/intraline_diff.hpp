#pragma once

#include <QStringView>
#include <QVector>

namespace GitNaga {

// A half-open character range [start, start + length) inside one line.
struct TextRange {
    int start = 0;
    int length = 0;
    friend bool operator==(const TextRange &, const TextRange &) = default;
};

// Which characters of a replaced line changed, per side.
struct IntralineChanges {
    QVector<TextRange> removed; // over the old line
    QVector<TextRange> added;   // over the new line
};

namespace detail {

// Words (letters, digits, underscore), whitespace runs, and every other
// character on its own: the unit a person reads a code edit in.
QVector<TextRange> tokenize(QStringView line);

} // namespace detail

// Token-level LCS between a deleted line and the addition that replaced it.
// Returns nothing for lines that share too little to make word marks useful:
// a fully rewritten line reads better as one block than as confetti.
IntralineChanges intralineDiff(QStringView before, QStringView after);

} // namespace GitNaga
