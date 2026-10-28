#include "conflict_parser.hpp"

#include <QTest>

using namespace GitNaga;
using conflicts::Choice;

// The conflict view rewrites a person's file, so the parser must hand back
// exactly what git wrote except where a choice was made.
class ConflictParserTest final : public QObject
{
    Q_OBJECT

    static QString twoWay()
    {
        return QStringLiteral("head\n<<<<<<< HEAD\nours 1\nours 2\n=======\ntheirs\n>>>>>>> other\ntail\n");
    }

private slots:
    void plainFileHasNoConflicts()
    {
        const auto text = QStringLiteral("a\n=======\nb\n");
        const auto document = conflicts::parse(text);
        QCOMPARE(document.conflictCount(), 0);
        QCOMPARE(document.blocks.size(), 1);
        QCOMPARE(conflicts::resolve(document, {}), text);
    }

    void splitsTwoWayConflict()
    {
        const auto document = conflicts::parse(twoWay());
        QCOMPARE(document.blocks.size(), 3);
        QCOMPARE(document.conflictCount(), 1);
        const auto &block = document.blocks.at(1);
        QVERIFY(block.conflict);
        QVERIFY(!block.hasBase);
        QCOMPARE(block.ours, QStringLiteral("ours 1\nours 2\n"));
        QCOMPARE(block.theirs, QStringLiteral("theirs\n"));
        QCOMPARE(block.oursLabel, QStringLiteral("HEAD"));
        QCOMPARE(block.theirsLabel, QStringLiteral("other"));
        QCOMPARE(document.blocks.at(0).text, QStringLiteral("head\n"));
        QCOMPARE(document.blocks.at(2).text, QStringLiteral("tail\n"));
    }

    void resolvesEveryChoice()
    {
        const auto document = conflicts::parse(twoWay());
        QCOMPARE(conflicts::resolve(document, { Choice::Ours }), QStringLiteral("head\nours 1\nours 2\ntail\n"));
        QCOMPARE(conflicts::resolve(document, { Choice::Theirs }), QStringLiteral("head\ntheirs\ntail\n"));
        QCOMPARE(conflicts::resolve(document, { Choice::OursThenTheirs }),
                 QStringLiteral("head\nours 1\nours 2\ntheirs\ntail\n"));
        QCOMPARE(conflicts::resolve(document, { Choice::TheirsThenOurs }),
                 QStringLiteral("head\ntheirs\nours 1\nours 2\ntail\n"));
    }

    void unresolvedKeepsMarkersVerbatim()
    {
        QCOMPARE(conflicts::resolve(conflicts::parse(twoWay()), { Choice::Unresolved }), twoWay());
        QCOMPARE(conflicts::resolve(conflicts::parse(twoWay()), {}), twoWay());
    }

    void readsDiff3Base()
    {
        const auto text = QStringLiteral("<<<<<<< ours\nO\n||||||| base\nB\n=======\nT\n>>>>>>> theirs\n");
        const auto document = conflicts::parse(text);
        QCOMPARE(document.conflictCount(), 1);
        const auto &block = document.blocks.at(0);
        QVERIFY(block.hasBase);
        QCOMPARE(block.base, QStringLiteral("B\n"));
        QCOMPARE(block.baseLabel, QStringLiteral("base"));
        QCOMPARE(block.ours, QStringLiteral("O\n"));
        QCOMPARE(block.theirs, QStringLiteral("T\n"));
        QCOMPARE(conflicts::resolve(document, { Choice::Theirs }), QStringLiteral("T\n"));
        QCOMPARE(conflicts::resolve(document, {}), text);
    }

    void choicesApplyInOrder()
    {
        const auto text = QStringLiteral("<<<<<<< a\n1\n=======\n2\n>>>>>>> b\nmid\n<<<<<<< a\n3\n=======\n4\n>>>>>>> b\n");
        const auto document = conflicts::parse(text);
        QCOMPARE(document.conflictCount(), 2);
        QCOMPARE(conflicts::resolve(document, { Choice::Theirs, Choice::Ours }), QStringLiteral("2\nmid\n3\n"));
        // A partial resolution leaves the second conflict for later.
        QCOMPARE(conflicts::resolve(document, { Choice::Ours }),
                 QStringLiteral("1\nmid\n<<<<<<< a\n3\n=======\n4\n>>>>>>> b\n"));
    }

    void keepsWindowsLineEndings()
    {
        const auto text = QStringLiteral("x\r\n<<<<<<< HEAD\r\nO\r\n=======\r\nT\r\n>>>>>>> b\r\ny\r\n");
        const auto document = conflicts::parse(text);
        QCOMPARE(document.conflictCount(), 1);
        QCOMPARE(conflicts::resolve(document, { Choice::Ours }), QStringLiteral("x\r\nO\r\ny\r\n"));
        QCOMPARE(conflicts::resolve(document, {}), text);
    }

    void emptySideAndMissingFinalNewline()
    {
        const auto text = QStringLiteral("<<<<<<< HEAD\n=======\nadded\n>>>>>>> b");
        const auto document = conflicts::parse(text);
        QCOMPARE(document.conflictCount(), 1);
        QCOMPARE(document.blocks.at(0).ours, QString());
        QCOMPARE(conflicts::resolve(document, { Choice::Ours }), QString());
        QCOMPARE(conflicts::resolve(document, { Choice::Theirs }), QStringLiteral("added\n"));
    }

    void unterminatedBlockIsText()
    {
        const auto text = QStringLiteral("a\n<<<<<<< HEAD\nb\n=======\nc\n");
        const auto document = conflicts::parse(text);
        QCOMPARE(document.conflictCount(), 0);
        QCOMPARE(conflicts::resolve(document, {}), text);
    }

    void wideMarkersMustMatch()
    {
        // conflict-marker-size=9: seven-wide runs inside are plain content.
        const auto text = QStringLiteral("<<<<<<<<< HEAD\n=======\nours\n=========\ntheirs\n>>>>>>>>> b\n");
        const auto document = conflicts::parse(text);
        QCOMPARE(document.conflictCount(), 1);
        QCOMPARE(document.blocks.at(0).ours, QStringLiteral("=======\nours\n"));
        QCOMPARE(document.blocks.at(0).theirs, QStringLiteral("theirs\n"));
    }

    void markerNeedsSpaceBeforeLabel()
    {
        const auto text = QStringLiteral("<<<<<<<x\n=======\n>>>>>>>\n");
        QCOMPARE(conflicts::parse(text).conflictCount(), 0);
    }
};

QTEST_GUILESS_MAIN(ConflictParserTest)
#include "conflict_parser_test.moc"
