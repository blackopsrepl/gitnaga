#include "diff_document.hpp"
#include "git_parse.hpp"

#include <QElapsedTimer>
#include <QTest>

using namespace GitNaga;

namespace {

QString marked(const QString &line, const QVector<TextRange> &ranges)
{
    QString result;
    int cursor = 0;
    for (const auto &range : ranges) {
        result += line.mid(cursor, range.start - cursor) + QLatin1Char('[') + line.mid(range.start, range.length)
                  + QLatin1Char(']');
        cursor = range.start + range.length;
    }
    return result + line.mid(cursor);
}

DiffDocument document(const QByteArray &patch, const QString &path = QStringLiteral("f.txt"))
{
    return buildDiffDocument(detail::parseDiff(patch), path);
}

} // namespace

class DiffAlgorithmsTest final : public QObject
{
    Q_OBJECT

private slots:
    void tokenizesWordsSpacesAndPunctuation()
    {
        const auto tokens = detail::tokenize(u"a_1  += foo(x);");
        QStringList texts;
        for (const auto &token : tokens)
            texts.append(QStringLiteral("a_1  += foo(x);").mid(token.start, token.length));
        QCOMPARE(texts, (QStringList{ QStringLiteral("a_1"), QStringLiteral("  "), QStringLiteral("+"),
                                      QStringLiteral("="), QStringLiteral(" "), QStringLiteral("foo"),
                                      QStringLiteral("("), QStringLiteral("x"), QStringLiteral(")"),
                                      QStringLiteral(";") }));
    }

    void marksOnlyTheChangedWord()
    {
        const QString before = QStringLiteral("    return compute(alpha, beta);");
        const QString after = QStringLiteral("    return compute(alpha, gamma);");
        const auto changes = intralineDiff(before, after);
        QCOMPARE(marked(before, changes.removed), QStringLiteral("    return compute(alpha, [beta]);"));
        QCOMPARE(marked(after, changes.added), QStringLiteral("    return compute(alpha, [gamma]);"));
    }

    void bridgesSpacesBetweenChangedWords()
    {
        const auto changes = intralineDiff(u"let x = old value here;", u"let x = new thing here;");
        QCOMPARE(marked(QStringLiteral("let x = new thing here;"), changes.added),
                 QStringLiteral("let x = [new thing] here;"));
    }

    void marksInsertionWithNothingRemoved()
    {
        const auto changes = intralineDiff(u"call(a)", u"call(a, b)");
        QVERIFY(changes.removed.isEmpty());
        QCOMPARE(marked(QStringLiteral("call(a, b)"), changes.added), QStringLiteral("call(a[, b])"));
    }

    void leavesRewritesUnmarked()
    {
        const auto changes = intralineDiff(u"completely different", u"nothing alike at all here");
        QVERIFY(changes.removed.isEmpty());
        QVERIFY(changes.added.isEmpty());
    }

    void capsTheCostOfVeryLongLines()
    {
        QString before, after;
        for (int i = 0; i < 4000; ++i) {
            before += QStringLiteral("w%1 ").arg(i);
            after += QStringLiteral("w%1 ").arg(i == 1000 || i == 3000 ? -i : i);
        }
        QElapsedTimer timer;
        timer.start();
        const auto changes = intralineDiff(before, after);
        QVERIFY(timer.elapsed() < 200);
        // Past the budget the middle is one block between prefix and suffix.
        QCOMPARE(changes.added.size(), 1);
    }

    void pairsDeletionsWithTheAdditionsAfterThem()
    {
        const auto doc = document("diff --git a/f.txt b/f.txt\n--- a/f.txt\n+++ b/f.txt\n@@ -1,4 +1,4 @@\n"
                                  " keep\n-one two\n-three four\n+one 2\n+three four five\n+extra\n tail\n");
        QCOMPARE(doc.hunkCount, 1);
        QCOMPARE(doc.rows.size(), 8);
        QCOMPARE(doc.rows.at(2).pair, 4);
        QCOMPARE(doc.rows.at(4).pair, 2);
        QCOMPARE(doc.rows.at(6).pair, -1);
        QCOMPARE(marked(doc.rows.at(4).content, doc.rows.at(4).style.changed), QStringLiteral("one [2]"));
        const QVector<SplitRow> expected = { { 0, 0, 0 }, { 0, 1, 1 }, { 0, 2, 4 }, { 0, 3, 5 }, { 0, -1, 6 }, { 0, 7, 7 } };
        QCOMPARE(doc.split, expected);
        QCOMPARE(doc.meta.additions, 3);
        QCOMPARE(doc.meta.deletions, 2);
    }

    void keepsNoNewlineNotesOnTheirSide()
    {
        const auto doc = document("diff --git a/f b/f\n@@ -1 +1 @@\n-old\n\\ No newline at end of file\n+new\n");
        const QVector<SplitRow> expected = { { 0, 0, 0 }, { 0, 1, 3 }, { 0, 2, -1 } };
        QCOMPARE(doc.split, expected);
        QCOMPARE(doc.rows.at(1).pair, 3);
    }

    void readsRenameAndModeFromTheHeader()
    {
        const auto doc = document("diff --git a/old.sh b/new.sh\nold mode 100644\nnew mode 100755\n"
                                  "similarity index 91%\nrename from old.sh\nrename to new.sh\n"
                                  "index 1111111..2222222\n--- a/old.sh\n+++ b/new.sh\n@@ -1 +1 @@\n-a\n+b\n",
                                  QStringLiteral("new.sh"));
        QCOMPARE(doc.meta.oldMode, QStringLiteral("100644"));
        QCOMPARE(doc.meta.newMode, QStringLiteral("100755"));
        QCOMPARE(doc.meta.similarity, 91);
        QVERIFY(doc.meta.renamed);
        QCOMPARE(doc.meta.oldPath, QStringLiteral("old.sh"));
        QCOMPARE(doc.rows.size(), 3);
    }

    void treatsTripleDashContentAsContent()
    {
        const auto doc = document("diff --git a/a.md b/a.md\n--- a/a.md\n+++ b/a.md\n@@ -1,2 +1,1 @@\n--- \n keep\n");
        QCOMPARE(doc.rows.at(1).kind, DiffLine::Kind::Deletion);
        QCOMPARE(doc.meta.deletions, 1);
    }

    void expandsTabsToColumns()
    {
        QCOMPARE(detail::expandTabs(u"\tx\ty"), QStringLiteral("    x   y"));
    }

    void buildsLargeDiffsQuickly()
    {
        QByteArray patch = "diff --git a/big.cpp b/big.cpp\n--- a/big.cpp\n+++ b/big.cpp\n@@ -1,10000 +1,10000 @@\n";
        for (int i = 0; i < 10000; ++i) {
            patch += "-    int value" + QByteArray::number(i) + " = compute(\"old\", " + QByteArray::number(i) + ");\n";
            patch += "+    int value" + QByteArray::number(i) + " = compute(\"new\", " + QByteArray::number(i) + ");\n";
        }
        QElapsedTimer timer;
        timer.start();
        const auto doc = document(patch, QStringLiteral("big.cpp"));
        QCOMPARE(doc.rows.size(), 20001);
        QVERIFY2(timer.elapsed() < 1500, qPrintable(QString::number(timer.elapsed())));
    }
};

QTEST_GUILESS_MAIN(DiffAlgorithmsTest)
#include "diff_algorithms_test.moc"
