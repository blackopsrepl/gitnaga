#pragma once

#include <QString>
#include <QVector>

namespace GitNaga::conflicts {

// How one conflict block is settled. "Both" keeps both sides, in the order the
// person picked, because a merge of two additions usually wants both lines.
enum class Choice { Unresolved, Ours, Theirs, OursThenTheirs, TheirsThenOurs };

// One stretch of a conflicted file: either plain text both sides agree on, or
// a conflict with each side's lines. Every string keeps its line endings, so
// writing the blocks back reproduces the file byte for byte.
struct Block {
    bool conflict = false;
    QString text;
    QString ours;
    QString base;
    QString theirs;
    QString oursLabel;
    QString baseLabel;
    QString theirsLabel;
    // The block exactly as git wrote it, markers included.
    QString raw;
    bool hasBase = false;
};

struct Document {
    QVector<Block> blocks;
    int conflictCount() const;
};

// Splits a working file into text and conflict blocks. A marker block that is
// never closed is not a conflict git wrote, so it stays text.
Document parse(const QString &text);

// The file with each conflict replaced by its choice, one choice per conflict
// in order. A conflict left Unresolved (or without a choice) keeps its markers.
QString resolve(const Document &document, const QVector<Choice> &choices);

// The text a single resolved conflict becomes.
QString resolvedText(const Block &block, Choice choice);

} // namespace GitNaga::conflicts
