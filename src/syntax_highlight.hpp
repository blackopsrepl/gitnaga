#pragma once

#include <QSet>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QVector>

namespace GitNaga {

enum class SyntaxKind : quint8 { Plain, Keyword, Type, Function, String, Number, Constant, Comment, Meta };

struct SyntaxSpan {
    int start = 0;
    int length = 0;
    SyntaxKind kind = SyntaxKind::Plain;
    friend bool operator==(const SyntaxSpan &, const SyntaxSpan &) = default;
};

// What a language looks like to a small scanner: enough to colour keywords,
// strings, comments and numbers, not to parse.
struct LanguageRules {
    QString name;
    QSet<QString> keywords;
    QSet<QString> types;
    QSet<QString> constants;
    QStringList lineComments;
    QString blockOpen;
    QString blockClose;
    QString quotes = QStringLiteral("\"'");
    bool tripleQuotes = false;       // Python """ docstrings
    bool capitalizedTypes = false;   // Foo is a type name
    bool preprocessor = false;       // a leading # is a directive (C, C++)
    bool decorators = false;         // @name is an annotation
    bool sigils = false;             // $var, ${var} (shell), @ivar / :sym (Ruby)
    bool bareKeys = false;           // key: (YAML) or key = (TOML) at line start
    bool sections = false;           // [section] headers (TOML)
    bool markdown = false;
};

// Carried from one line to the next so a block comment or a docstring
// keeps its colour across lines. A diff keeps one state per side.
struct HighlightState {
    enum class Block : quint8 { None, Comment, TripleDouble, TripleSingle, Fence };
    Block block = Block::None;
    friend bool operator==(const HighlightState &, const HighlightState &) = default;
};

// Null when the path is not a language the highlighter knows.
const LanguageRules *languageForPath(const QString &path);

QVector<SyntaxSpan> highlightLine(const LanguageRules *rules, QStringView line, HighlightState &state);

} // namespace GitNaga
