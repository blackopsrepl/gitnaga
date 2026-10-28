#include "syntax_highlight.hpp"

#include <QTest>

using namespace GitNaga;

namespace {

// "kind:text" for each span, the way a reader would describe the colours.
QStringList describe(const QString &path, const QString &line, HighlightState &state)
{
    static const QStringList names = { QStringLiteral("plain"), QStringLiteral("keyword"), QStringLiteral("type"),
                                       QStringLiteral("function"), QStringLiteral("string"), QStringLiteral("number"),
                                       QStringLiteral("constant"), QStringLiteral("comment"), QStringLiteral("meta") };
    QStringList result;
    for (const auto &span : highlightLine(languageForPath(path), line, state))
        result.append(names.at(static_cast<int>(span.kind)) + QLatin1Char(':') + line.mid(span.start, span.length));
    return result;
}

QStringList describe(const QString &path, const QString &line)
{
    HighlightState state;
    return describe(path, line, state);
}

} // namespace

class SyntaxTest final : public QObject
{
    Q_OBJECT

private slots:
    void picksLanguagesByPath()
    {
        QCOMPARE(languageForPath(QStringLiteral("src/a.hpp"))->name, QStringLiteral("C++"));
        QCOMPARE(languageForPath(QStringLiteral("x/CMakeLists.txt"))->name, QStringLiteral("CMake"));
        QCOMPARE(languageForPath(QStringLiteral("Gemfile"))->name, QStringLiteral("Ruby"));
        QCOMPARE(languageForPath(QStringLiteral("a.TSX"))->name, QStringLiteral("JavaScript"));
        QVERIFY(!languageForPath(QStringLiteral("notes.txt")));
        HighlightState none;
        QVERIFY(highlightLine(nullptr, u"int x;", none).isEmpty());
    }

    void coloursCpp()
    {
        QCOMPARE(describe(QStringLiteral("a.cpp"), QStringLiteral("return count(\"x\", 42); // done")),
                 (QStringList{ QStringLiteral("keyword:return"), QStringLiteral("function:count"),
                               QStringLiteral("string:\"x\""), QStringLiteral("number:42"),
                               QStringLiteral("comment:// done") }));
        QCOMPARE(describe(QStringLiteral("a.cpp"), QStringLiteral("  #include <vector>")),
                 QStringList{ QStringLiteral("meta:#include <vector>") });
        QCOMPARE(describe(QStringLiteral("a.cpp"), QStringLiteral("const QString s = \"a\\\"b\";")),
                 (QStringList{ QStringLiteral("keyword:const"), QStringLiteral("type:QString"),
                               QStringLiteral("string:\"a\\\"b\"") }));
    }

    void carriesBlockCommentsAcrossLines()
    {
        HighlightState state;
        QCOMPARE(describe(QStringLiteral("a.rs"), QStringLiteral("let x = 1; /* start"), state).last(),
                 QStringLiteral("comment:/* start"));
        QCOMPARE(state.block, HighlightState::Block::Comment);
        QCOMPARE(describe(QStringLiteral("a.rs"), QStringLiteral("still end */ fn"), state),
                 (QStringList{ QStringLiteral("comment:still end */"), QStringLiteral("keyword:fn") }));
        QCOMPARE(state.block, HighlightState::Block::None);
    }

    void coloursPython()
    {
        QCOMPARE(describe(QStringLiteral("a.py"), QStringLiteral("@cache")), QStringList{ QStringLiteral("meta:@cache") });
        QCOMPARE(describe(QStringLiteral("a.py"), QStringLiteral("def f(x=None):  # hi")),
                 (QStringList{ QStringLiteral("keyword:def"), QStringLiteral("function:f"),
                               QStringLiteral("constant:None"), QStringLiteral("comment:# hi") }));
        HighlightState state;
        describe(QStringLiteral("a.py"), QStringLiteral("    \"\"\"Doc"), state);
        QCOMPARE(state.block, HighlightState::Block::TripleDouble);
        QCOMPARE(describe(QStringLiteral("a.py"), QStringLiteral("more\"\"\" + 1"), state),
                 (QStringList{ QStringLiteral("string:more\"\"\""), QStringLiteral("number:1") }));
    }

    void coloursDataFormats()
    {
        QCOMPARE(describe(QStringLiteral("a.json"), QStringLiteral("  \"name\": \"naga\", \"n\": true")),
                 (QStringList{ QStringLiteral("type:\"name\""), QStringLiteral("string:\"naga\""),
                               QStringLiteral("type:\"n\""), QStringLiteral("constant:true") }));
        QCOMPARE(describe(QStringLiteral("ci.yml"), QStringLiteral("  - run-on: linux # note")),
                 (QStringList{ QStringLiteral("type:run-on"), QStringLiteral("comment:# note") }));
        QCOMPARE(describe(QStringLiteral("Cargo.toml"), QStringLiteral("[package]")),
                 QStringList{ QStringLiteral("meta:[package]") });
        QCOMPARE(describe(QStringLiteral("Cargo.toml"), QStringLiteral("version = \"1.0\"")),
                 (QStringList{ QStringLiteral("type:version"), QStringLiteral("string:\"1.0\"") }));
    }

    void coloursShellAndRuby()
    {
        QCOMPARE(describe(QStringLiteral("x.sh"), QStringLiteral("if [ -n \"$x\" ]; then echo ${HOME}; fi")),
                 (QStringList{ QStringLiteral("keyword:if"), QStringLiteral("string:\"$x\""),
                               QStringLiteral("keyword:then"), QStringLiteral("meta:${HOME}"),
                               QStringLiteral("keyword:fi") }));
        QCOMPARE(describe(QStringLiteral("a.rb"), QStringLiteral("attr_reader :name, @x")),
                 (QStringList{ QStringLiteral("keyword:attr_reader"), QStringLiteral("meta::name"),
                               QStringLiteral("meta:@x") }));
    }

    void coloursMarkdown()
    {
        HighlightState state;
        QCOMPARE(describe(QStringLiteral("README.md"), QStringLiteral("## Build"), state),
                 QStringList{ QStringLiteral("keyword:## Build") });
        QCOMPARE(describe(QStringLiteral("README.md"), QStringLiteral("Run `make` [here](x.md)"), state),
                 (QStringList{ QStringLiteral("string:`make`"), QStringLiteral("function:(x.md)") }));
        describe(QStringLiteral("README.md"), QStringLiteral("```cpp"), state);
        QCOMPARE(describe(QStringLiteral("README.md"), QStringLiteral("int x;"), state),
                 QStringList{ QStringLiteral("string:int x;") });
    }
};

QTEST_GUILESS_MAIN(SyntaxTest)
#include "syntax_test.moc"
