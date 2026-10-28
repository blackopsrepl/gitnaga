#include "syntax_highlight.hpp"

#include <algorithm>

namespace GitNaga {

namespace {

using Block = HighlightState::Block;

struct Scanner {
    const LanguageRules &rules;
    QStringView line;
    HighlightState &state;
    QVector<SyntaxSpan> spans;
    int pos = 0;

    int size() const { return static_cast<int>(line.size()); }
    QChar at(int index) const { return index < size() ? line.at(index) : QChar(); }
    bool startsWith(const QString &token, int index) const
    {
        return !token.isEmpty() && line.mid(index).startsWith(token);
    }
    void mark(int start, int end, SyntaxKind kind)
    {
        if (end > start)
            spans.append({ start, end - start, kind });
    }
    int firstVisible() const
    {
        int index = 0;
        while (index < size() && line.at(index).isSpace())
            ++index;
        return index;
    }

    // Finds `close` from `from`; marks through it, or to the end of the line
    // and keeps the block open for the next line.
    void continueBlock(int from, const QString &close, SyntaxKind kind, Block block)
    {
        const auto end = line.indexOf(close, from);
        if (end < 0) {
            mark(from, size(), kind);
            pos = size();
            state.block = block;
            return;
        }
        mark(from, static_cast<int>(end + close.size()), kind);
        pos = static_cast<int>(end + close.size());
        state.block = Block::None;
    }

    void string(QChar quote)
    {
        if (rules.tripleQuotes && line.mid(pos, 3) == QString(3, quote)) {
            const auto block = quote == QLatin1Char('"') ? Block::TripleDouble : Block::TripleSingle;
            const int from = pos;
            pos += 3;
            continueBlock(pos, QString(3, quote), SyntaxKind::String, block);
            spans.last().start = from;
            spans.last().length += 3;
            return;
        }
        int end = pos + 1;
        while (end < size() && line.at(end) != quote)
            end += line.at(end) == QLatin1Char('\\') ? 2 : 1;
        end = std::min(end + 1, size());
        const bool key = rules.bareKeys || rules.name == QStringLiteral("JSON");
        int after = end;
        while (after < size() && line.at(after).isSpace())
            ++after;
        mark(pos, end, key && at(after) == QLatin1Char(':') ? SyntaxKind::Type : SyntaxKind::String);
        pos = end;
    }

    void number()
    {
        int end = pos;
        while (end < size() && (line.at(end).isLetterOrNumber() || line.at(end) == QLatin1Char('.')
                                || line.at(end) == QLatin1Char('_')))
            ++end;
        mark(pos, end, SyntaxKind::Number);
        pos = end;
    }

    void identifier()
    {
        int end = pos;
        while (end < size() && (line.at(end).isLetterOrNumber() || line.at(end) == QLatin1Char('_')))
            ++end;
        if (at(end) == QLatin1Char('?') && rules.sigils)
            ++end;
        const auto word = line.mid(pos, end - pos).toString();
        int after = end;
        while (after < size() && line.at(after) == QLatin1Char(' '))
            ++after;
        SyntaxKind kind = SyntaxKind::Plain;
        if (rules.keywords.contains(word))
            kind = SyntaxKind::Keyword;
        else if (rules.constants.contains(word))
            kind = SyntaxKind::Constant;
        else if (rules.types.contains(word) || (rules.capitalizedTypes && word.at(0).isUpper()))
            kind = SyntaxKind::Type;
        else if (at(after) == QLatin1Char('(') || (at(end) == QLatin1Char('!') && rules.name == QStringLiteral("Rust")))
            kind = SyntaxKind::Function;
        if (kind != SyntaxKind::Plain)
            mark(pos, end, kind);
        pos = end;
    }

    bool sigil()
    {
        const auto c = at(pos);
        const bool shellVariable = c == QLatin1Char('$');
        const bool rubySymbol = (c == QLatin1Char('@') || c == QLatin1Char(':')) && at(pos + 1).isLetter()
                                && at(pos - 1) != QLatin1Char(':');
        if (!shellVariable && !rubySymbol)
            return false;
        int end = pos + 1;
        if (shellVariable && at(end) == QLatin1Char('{')) {
            const auto close = line.indexOf(QLatin1Char('}'), end);
            end = close < 0 ? size() : static_cast<int>(close + 1);
        } else {
            while (end < size() && (line.at(end).isLetterOrNumber() || line.at(end) == QLatin1Char('_')))
                ++end;
        }
        mark(pos, end, SyntaxKind::Meta);
        pos = end;
        return true;
    }

    // `key:` (YAML) or `key =` (TOML) at the start of a line, and [section].
    void lineStart()
    {
        int index = firstVisible();
        if (rules.sections && at(index) == QLatin1Char('[')) {
            mark(index, size(), SyntaxKind::Meta);
            pos = size();
            return;
        }
        if (at(index) == QLatin1Char('-') && at(index + 1) == QLatin1Char(' '))
            index += 2;
        int end = index;
        while (end < size() && (line.at(end).isLetterOrNumber() || QStringLiteral("_-.").contains(line.at(end))))
            ++end;
        int after = end;
        while (after < size() && line.at(after) == QLatin1Char(' '))
            ++after;
        if (end > index && (at(after) == QLatin1Char(':') || at(after) == QLatin1Char('='))) {
            mark(index, end, SyntaxKind::Type);
            pos = end;
        }
    }

    bool lineComment()
    {
        for (const auto &token : rules.lineComments) {
            const bool hash = token == QStringLiteral("#");
            if (startsWith(token, pos) && (!hash || pos == 0 || line.at(pos - 1).isSpace())) {
                mark(pos, size(), SyntaxKind::Comment);
                pos = size();
                return true;
            }
        }
        return false;
    }

    void scan()
    {
        if (state.block == Block::Comment)
            continueBlock(0, rules.blockClose, SyntaxKind::Comment, Block::Comment);
        else if (state.block == Block::TripleDouble)
            continueBlock(0, QStringLiteral("\"\"\""), SyntaxKind::String, Block::TripleDouble);
        else if (state.block == Block::TripleSingle)
            continueBlock(0, QStringLiteral("'''"), SyntaxKind::String, Block::TripleSingle);
        if (pos == 0 && rules.preprocessor && at(firstVisible()) == QLatin1Char('#')) {
            mark(firstVisible(), size(), SyntaxKind::Meta);
            return;
        }
        if (pos == 0 && rules.bareKeys)
            lineStart();
        while (pos < size()) {
            const auto c = line.at(pos);
            if (c.isSpace()) {
                ++pos;
            } else if (lineComment()) {
            } else if (startsWith(rules.blockOpen, pos)) {
                continueBlock(pos, rules.blockClose, SyntaxKind::Comment, Block::Comment);
            } else if (rules.quotes.contains(c)) {
                string(c);
            } else if (c.isDigit()) {
                number();
            } else if (c.isLetter() || c == QLatin1Char('_')) {
                identifier();
            } else if (rules.decorators && c == QLatin1Char('@') && at(pos + 1).isLetter()) {
                const int from = pos++;
                identifier();
                mark(from, pos, SyntaxKind::Meta);
            } else if (!(rules.sigils && sigil())) {
                ++pos;
            }
        }
    }
};

QVector<SyntaxSpan> markdownLine(QStringView line, HighlightState &state)
{
    QVector<SyntaxSpan> spans;
    const auto size = static_cast<int>(line.size());
    const auto trimmed = line.trimmed();
    if (trimmed.startsWith(QStringLiteral("```"))) {
        state.block = state.block == Block::Fence ? Block::None : Block::Fence;
        return { { 0, size, SyntaxKind::Meta } };
    }
    if (state.block == Block::Fence)
        return { { 0, size, SyntaxKind::String } };
    if (trimmed.startsWith(QLatin1Char('#')))
        return { { 0, size, SyntaxKind::Keyword } };
    if (trimmed.startsWith(QLatin1Char('>')))
        return { { 0, size, SyntaxKind::Comment } };
    for (int index = 0; index < size; ++index) {
        const auto c = line.at(index);
        const QChar close = c == QLatin1Char('`') ? QLatin1Char('`') : c == QLatin1Char('(') && index > 0
                                                                           && line.at(index - 1) == QLatin1Char(']')
                                                                           ? QLatin1Char(')') : QChar();
        if (close.isNull())
            continue;
        const auto end = line.indexOf(close, index + 1);
        if (end < 0)
            break;
        spans.append({ index, static_cast<int>(end) - index + 1, c == QLatin1Char('`') ? SyntaxKind::String : SyntaxKind::Function });
        index = static_cast<int>(end);
    }
    return spans;
}

} // namespace

QVector<SyntaxSpan> highlightLine(const LanguageRules *rules, QStringView line, HighlightState &state)
{
    if (!rules)
        return {};
    if (rules->markdown)
        return markdownLine(line, state);
    Scanner scanner{ *rules, line, state, {} };
    scanner.scan();
    return scanner.spans;
}

} // namespace GitNaga
