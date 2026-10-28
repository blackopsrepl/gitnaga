#include "blame_model.hpp"
#include "commit_search.hpp"
#include "git_navigation.hpp"
#include "navigation_controller.hpp"

#include <QTest>

using namespace GitNaga;

// Pure parsing and matching behind search, blame and file history: the parts
// that decide what a person sees, checked against git's real output shapes.
class NavigationParseTest final : public QObject
{
    Q_OBJECT

    static QByteArray porcelain()
    {
        const QByteArray first(40, 'a');
        const QByteArray second(40, 'b');
        const QByteArray zero(40, '0');
        return first + " 1 1 2\nauthor Ada Lovelace\nauthor-mail <ada@zoo.invalid>\nauthor-time 1600000000\n"
                       "author-tz +0000\ncommitter Ada Lovelace\ncommitter-mail <ada@zoo.invalid>\n"
                       "committer-time 1600000000\ncommitter-tz +0000\nsummary first\nboundary\nfilename f.txt\n"
                       "\tline one\n"
             + first + " 2 2\n\tauthor of line two\n"
             + second + " 3 3 1\nauthor Grace Hopper\nauthor-mail <grace@zoo.invalid>\nauthor-time 1600086400\n"
                        "author-tz +0000\nsummary second\nprevious " + first + " f.txt\nfilename f.txt\n\tline three\n"
             + first + " 3 4 1\nfilename f.txt\n\tline four\n"
             + zero + " 5 5 1\nauthor Not Committed Yet\nauthor-mail <not.committed.yet>\n"
                      "author-time 1700000000\nauthor-tz +0000\nsummary Version of f.txt from f.txt\n"
                      "filename f.txt\n\t\n";
    }

private slots:
    void blamePorcelainCarriesCommitDetailsToEveryLine()
    {
        const auto lines = detail::parseBlamePorcelain(porcelain());
        QCOMPARE(lines.size(), 5);
        QCOMPARE(lines.at(0).author, QStringLiteral("Ada Lovelace"));
        QCOMPARE(lines.at(0).authorEmail, QStringLiteral("ada@zoo.invalid"));
        QCOMPARE(lines.at(0).summary, QStringLiteral("first"));
        QCOMPARE(lines.at(0).text, QStringLiteral("line one"));
        // The second line names its commit by oid only.
        QCOMPARE(lines.at(1).author, QStringLiteral("Ada Lovelace"));
        QCOMPARE(lines.at(1).text, QStringLiteral("author of line two"));
        QCOMPARE(lines.at(1).line, 2);
        QCOMPARE(lines.at(2).author, QStringLiteral("Grace Hopper"));
        QCOMPARE(lines.at(2).authoredAt.toSecsSinceEpoch(), 1600086400);
        QCOMPARE(lines.at(3).originalLine, 3);
        QCOMPARE(lines.at(3).line, 4);
        QCOMPARE(lines.at(3).summary, QStringLiteral("first"));
        QVERIFY(!lines.at(3).uncommitted());
        QVERIFY(lines.at(4).uncommitted());
        QCOMPARE(lines.at(4).text, QString());
    }

    void blameGroupsConsecutiveLinesOfOneCommit()
    {
        BlameModel model;
        model.apply(detail::parseBlamePorcelain(porcelain()));
        QCOMPARE(model.count(), 5);
        QCOMPARE(model.commitCount(), 3);
        const auto at = [&model](int row, int role) { return model.data(model.index(row), role); };
        const QList<bool> starts{ true, false, true, true, true };
        const QList<int> groups{ 0, 0, 1, 2, 3 };
        for (int row = 0; row < 5; ++row) {
            QCOMPARE(at(row, BlameModel::GroupStartRole).toBool(), starts.at(row));
            QCOMPARE(at(row, BlameModel::GroupIndexRole).toInt(), groups.at(row));
        }
        QCOMPARE(at(0, BlameModel::ShortOidRole).toString(), QStringLiteral("aaaaaaa"));
        // Uncommitted lines point at no commit: a gutter click has nothing to select.
        QCOMPARE(at(4, BlameModel::OidRole).toString(), QString());
        QCOMPARE(at(4, BlameModel::AgeRole).toString(), QStringLiteral("uncommitted"));
    }

    void blameOfEmptyOutputIsEmpty()
    {
        QVERIFY(detail::parseBlamePorcelain({}).isEmpty());
        QVERIFY(detail::parseBlamePorcelain("garbage\n\tline\n").isEmpty());
    }

    void fileHistoryKeepsRenames()
    {
        const QByteArray log = "\x1e" + QByteArray(40, 'c') + "\x1f" "Ada\x1f" "1600000000\x1f" "edit b\n\nM\tdir/b.txt\n"
                               "\x1e" + QByteArray(40, 'd') + "\x1f" "Ada\x1f" "1500000000\x1f" "rename\n\nR093\ta.txt\tdir/b.txt\n"
                               "\x1e" + QByteArray(40, 'e') + "\x1f" "Ada\x1f" "1400000000\x1f" "add\n\nA\ta.txt\n";
        const auto revisions = detail::parseFileHistory(log);
        QCOMPARE(revisions.size(), 3);
        QCOMPARE(revisions.at(0).status, QStringLiteral("M"));
        QCOMPARE(revisions.at(0).path, QStringLiteral("dir/b.txt"));
        QCOMPARE(revisions.at(1).status, QStringLiteral("R"));
        QCOMPARE(revisions.at(1).oldPath, QStringLiteral("a.txt"));
        QCOMPARE(revisions.at(1).path, QStringLiteral("dir/b.txt"));
        QCOMPARE(revisions.at(2).path, QStringLiteral("a.txt"));
        QCOMPARE(revisions.at(2).subject, QStringLiteral("add"));
    }

    void ageIsShort()
    {
        const auto now = QDateTime::fromSecsSinceEpoch(1700000000);
        QCOMPARE(detail::shortAge(now.addSecs(-60), now), QStringLiteral("now"));
        QCOMPARE(detail::shortAge(now.addSecs(-5 * 3600), now), QStringLiteral("5h"));
        QCOMPARE(detail::shortAge(now.addDays(-3), now), QStringLiteral("3d"));
        QCOMPARE(detail::shortAge(now.addDays(-21), now), QStringLiteral("3w"));
        QCOMPARE(detail::shortAge(now.addDays(-150), now), QStringLiteral("5mo"));
        QCOMPARE(detail::shortAge(now.addDays(-800), now), QStringLiteral("2y"));
    }

    void queryPrefixesChooseTheKind()
    {
        QCOMPARE(parseSearchQuery(QStringLiteral("  fix bug ")).term, QStringLiteral("fix bug"));
        QVERIFY(parseSearchQuery(QStringLiteral("fix")).kind == SearchQuery::Kind::Text);
        const auto path = parseSearchQuery(QStringLiteral("path: src/*.cpp"));
        QVERIFY(path.kind == SearchQuery::Kind::Path);
        QCOMPARE(path.term, QStringLiteral("src/*.cpp"));
        QVERIFY(path.needsGit());
        QVERIFY(parseSearchQuery(QStringLiteral("Content:needle")).kind == SearchQuery::Kind::Content);
        QVERIFY(parseSearchQuery(QStringLiteral("author:ada")).kind == SearchQuery::Kind::Author);
        QVERIFY(!parseSearchQuery(QStringLiteral("author:ada")).needsGit());
        QVERIFY(parseSearchQuery(QStringLiteral("path:")).isEmpty());
    }

    void textMatchesSubjectAuthorEmailAndOidPrefix()
    {
        const SearchRecord record{ QStringLiteral("abcdef0123456789abcdef0123456789abcdef01"),
                                   QStringLiteral("feat(search): Add Search"), QStringLiteral("Ada Lovelace"),
                                   QStringLiteral("ada@zoo.invalid") };
        const auto text = [](const char *term) { return SearchQuery{ SearchQuery::Kind::Text, QString::fromUtf8(term) }; };
        QVERIFY(matchesRecord(record, text("add search")));
        QVERIFY(matchesRecord(record, text("LOVELACE")));
        QVERIFY(matchesRecord(record, text("zoo.invalid")));
        QVERIFY(matchesRecord(record, text("abcdef01")));
        QVERIFY(matchesRecord(record, text("ABCD")));
        // Too short to be meant as an oid, and not in any text field.
        QVERIFY(!matchesRecord(record, text("ab0")));
        QVERIFY(!matchesRecord(record, text("bcdef0")));
        QVERIFY(!matchesRecord(record, text("grace")));
        const SearchQuery author{ SearchQuery::Kind::Author, QStringLiteral("ada") };
        QVERIFY(matchesRecord(record, author));
        const SearchQuery authorMiss{ SearchQuery::Kind::Author, QStringLiteral("search") };
        QVERIFY(!matchesRecord(record, authorMiss));
    }

    void matchRecordsKeepsOrderAndStopsWhenCancelled()
    {
        QVector<SearchRecord> records;
        for (int index = 0; index < 20000; ++index)
            records.append({ QString::number(index), QStringLiteral("change %1").arg(index), QStringLiteral("Ada"), {} });
        const SearchQuery query{ SearchQuery::Kind::Text, QStringLiteral("change 1999") };
        QCOMPARE(matchRecords(records, query), (QStringList{ QStringLiteral("1999"), QStringLiteral("19990"),
                                                             QStringLiteral("19991"), QStringLiteral("19992"),
                                                             QStringLiteral("19993"), QStringLiteral("19994"),
                                                             QStringLiteral("19995"), QStringLiteral("19996"),
                                                             QStringLiteral("19997"), QStringLiteral("19998"),
                                                             QStringLiteral("19999") }));
        auto cancel = std::make_shared<std::atomic_bool>(true);
        QVERIFY(matchRecords(records, query, cancel).size() < 11);
        // Path and content questions are git's to answer, never the matcher's.
        QVERIFY(matchRecords(records, { SearchQuery::Kind::Path, QStringLiteral("change") }).isEmpty());
    }

    void commandRankingPrefersWholeWords()
    {
        const QStringList labels{ QStringLiteral("Previous commit"), QStringLiteral("Push"),
                                  QStringLiteral("Checkout feature/push-fix"), QStringLiteral("Refresh") };
        NavigationController navigation(nullptr);
        const auto order = navigation.rankCommands(labels, QStringLiteral("push"));
        QCOMPARE(order.size(), 2);
        QCOMPARE(order.at(0).toInt(), 1);
        QCOMPARE(order.at(1).toInt(), 2);
        QCOMPARE(navigation.rankCommands(labels, QString()).size(), labels.size());
    }
};

QTEST_GUILESS_MAIN(NavigationParseTest)

#include "navigation_parse_test.moc"
