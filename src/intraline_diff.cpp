#include "intraline_diff.hpp"

#include <algorithm>
#include <vector>

namespace GitNaga {

namespace {

// Above this many DP cells the edit is no longer a word tweak; fall back to
// the common prefix and suffix, which is what a reader scans for anyway.
constexpr qsizetype kCellBudget = 160 * 160;
// Lines sharing less than this fraction of their visible text are rewrites.
constexpr double kMinimumSimilarity = 0.3;

enum class CharClass { Word, Space, Other };

CharClass classify(QChar character)
{
    if (character.isLetterOrNumber() || character == QLatin1Char('_'))
        return CharClass::Word;
    if (character.isSpace())
        return CharClass::Space;
    return CharClass::Other;
}

struct Side {
    QStringView line;
    QVector<TextRange> tokens;
    std::vector<bool> changed;

    QStringView token(qsizetype index) const
    {
        const auto &range = tokens.at(index);
        return line.mid(range.start, range.length);
    }
    bool isSpace(qsizetype index) const { return line.at(tokens.at(index).start).isSpace(); }
};

void markByLcs(Side &a, Side &b, qsizetype from, qsizetype aEnd, qsizetype bEnd)
{
    // Both sides start at the shared prefix, which has the same length on each.
    const auto offset = static_cast<size_t>(from);
    const auto n = static_cast<size_t>(aEnd) - offset;
    const auto m = static_cast<size_t>(bEnd) - offset;
    std::vector<quint16> table((n + 1) * (m + 1), 0);
    const auto at = [m](size_t i, size_t j) { return i * (m + 1) + j; };
    for (size_t i = n; i-- > 0;) {
        for (size_t j = m; j-- > 0;) {
            const auto ai = static_cast<qsizetype>(offset + i);
            const auto bj = static_cast<qsizetype>(offset + j);
            table[at(i, j)] = a.token(ai) == b.token(bj)
                                  ? static_cast<quint16>(table[at(i + 1, j + 1)] + 1)
                                  : std::max(table[at(i + 1, j)], table[at(i, j + 1)]);
        }
    }
    size_t i = 0;
    size_t j = 0;
    while (i < n && j < m) {
        if (a.token(static_cast<qsizetype>(offset + i)) == b.token(static_cast<qsizetype>(offset + j))) {
            a.changed[offset + i++] = false;
            b.changed[offset + j++] = false;
        } else if (table[at(i + 1, j)] >= table[at(i, j + 1)]) {
            ++i;
        } else {
            ++j;
        }
    }
}

// Merges changed tokens into ranges; a whitespace gap between two changed
// tokens joins them, so "foo bar" -> "baz qux" is one mark, not two.
QVector<TextRange> collect(const Side &side)
{
    QVector<TextRange> ranges;
    const auto count = side.tokens.size();
    for (qsizetype index = 0; index < count; ++index) {
        if (!side.changed[static_cast<size_t>(index)])
            continue;
        const bool bridged = index >= 2 && side.isSpace(index - 1) && side.changed[static_cast<size_t>(index - 2)];
        const auto &token = side.tokens.at(index);
        if (!ranges.isEmpty() && (bridged || ranges.last().start + ranges.last().length == token.start))
            ranges.last().length = token.start + token.length - ranges.last().start;
        else
            ranges.append(token);
    }
    return ranges;
}

int visibleLength(QStringView text)
{
    return static_cast<int>(std::count_if(text.begin(), text.end(), [](QChar c) { return !c.isSpace(); }));
}

int unchangedVisible(const Side &side)
{
    int total = 0;
    for (qsizetype index = 0; index < side.tokens.size(); ++index) {
        if (!side.changed[static_cast<size_t>(index)])
            total += visibleLength(side.token(index));
    }
    return total;
}

} // namespace

namespace detail {

QVector<TextRange> tokenize(QStringView line)
{
    QVector<TextRange> tokens;
    const auto size = static_cast<int>(line.size());
    int index = 0;
    while (index < size) {
        const auto kind = classify(line.at(index));
        int end = index + 1;
        if (kind != CharClass::Other) {
            while (end < size && classify(line.at(end)) == kind)
                ++end;
        }
        tokens.append({ index, end - index });
        index = end;
    }
    return tokens;
}

} // namespace detail

IntralineChanges intralineDiff(QStringView before, QStringView after)
{
    Side a{ before, detail::tokenize(before), {} };
    Side b{ after, detail::tokenize(after), {} };
    a.changed.assign(static_cast<size_t>(a.tokens.size()), true);
    b.changed.assign(static_cast<size_t>(b.tokens.size()), true);

    qsizetype prefix = 0;
    while (prefix < a.tokens.size() && prefix < b.tokens.size() && a.token(prefix) == b.token(prefix)) {
        a.changed[static_cast<size_t>(prefix)] = false;
        b.changed[static_cast<size_t>(prefix)] = false;
        ++prefix;
    }
    qsizetype aEnd = a.tokens.size();
    qsizetype bEnd = b.tokens.size();
    while (aEnd > prefix && bEnd > prefix && a.token(aEnd - 1) == b.token(bEnd - 1)) {
        a.changed[static_cast<size_t>(--aEnd)] = false;
        b.changed[static_cast<size_t>(--bEnd)] = false;
    }
    if ((aEnd - prefix) * (bEnd - prefix) <= kCellBudget && aEnd > prefix && bEnd > prefix)
        markByLcs(a, b, prefix, aEnd, bEnd);

    const int total = visibleLength(before) + visibleLength(after);
    const int kept = unchangedVisible(a) + unchangedVisible(b);
    if (total == 0 || static_cast<double>(kept) / total < kMinimumSimilarity)
        return {};
    return { collect(a), collect(b) };
}

} // namespace GitNaga
