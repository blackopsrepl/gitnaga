#include "rebase_todo.hpp"

#include <QTest>

using namespace GitNaga;
using rebase::Action;

// The todo is the whole contract with git: these pin its exact text.
class RebaseTodoTest final : public QObject
{
    Q_OBJECT

    static rebase::Step step(Action action, const QString &oid, const QString &message = {})
    {
        return { action, oid, QStringLiteral("subject ") + oid, QStringLiteral("subject ") + oid
                                                                    + QStringLiteral("\n\nbody ") + oid + QLatin1Char('\n'),
                 message };
    }

    static QString exec(int number)
    {
        return QStringLiteral("exec git commit --amend --only --no-verify --allow-empty --cleanup=whitespace --quiet -F "
                              "'/m/message-%1'\n").arg(number);
    }

private slots:
    void picksPassThrough()
    {
        const auto todo = rebase::buildTodo({ step(Action::Pick, QStringLiteral("a")), step(Action::Edit, QStringLiteral("b")),
                                              step(Action::Drop, QStringLiteral("c")) },
                                            QStringLiteral("/m"));
        QVERIFY(todo.has_value());
        QCOMPARE(todo->text, QStringLiteral("pick a subject a\nedit b subject b\ndrop c subject c\n"));
        QVERIFY(todo->messages.isEmpty());
    }

    void rewordWritesItsMessageAfterThePick()
    {
        const auto todo = rebase::buildTodo({ step(Action::Reword, QStringLiteral("a"), QStringLiteral("new\n\nbody")),
                                              step(Action::Pick, QStringLiteral("b")) },
                                            QStringLiteral("/m"));
        QVERIFY(todo.has_value());
        QCOMPARE(todo->text, QStringLiteral("pick a subject a\n") + exec(1) + QStringLiteral("pick b subject b\n"));
        QCOMPARE(todo->messages.size(), 1);
        QCOMPARE(todo->messages.first().path, QStringLiteral("/m/message-1"));
        QCOMPARE(todo->messages.first().text, QStringLiteral("new\n\nbody"));
    }

    void unchangedRewordIsAPlainPick()
    {
        auto reword = step(Action::Reword, QStringLiteral("a"));
        reword.message = reword.original;
        const auto todo = rebase::buildTodo({ reword }, QStringLiteral("/m"));
        QVERIFY(todo.has_value());
        QCOMPARE(todo->text, QStringLiteral("pick a subject a\n"));
    }

    void squashCombinesMessagesWithoutAnEditor()
    {
        const auto todo = rebase::buildTodo({ step(Action::Pick, QStringLiteral("a")), step(Action::Squash, QStringLiteral("b")),
                                              step(Action::Fixup, QStringLiteral("c")) },
                                            QStringLiteral("/m"));
        QVERIFY(todo.has_value());
        QCOMPARE(todo->text, QStringLiteral("pick a subject a\nfixup b subject b\nfixup c subject c\n") + exec(1));
        QCOMPARE(todo->messages.first().text,
                 QStringLiteral("subject a\n\nbody a\n\nsubject b\n\nbody b\n"));
    }

    void editedSquashMessageWins()
    {
        const auto todo = rebase::buildTodo({ step(Action::Pick, QStringLiteral("a")),
                                              step(Action::Squash, QStringLiteral("b"), QStringLiteral("together")) },
                                            QStringLiteral("/m"));
        QVERIFY(todo.has_value());
        QCOMPARE(todo->messages.first().text, QStringLiteral("together"));
    }

    void fixupAloneKeepsTheHeadMessage()
    {
        const auto todo = rebase::buildTodo({ step(Action::Pick, QStringLiteral("a")), step(Action::Drop, QStringLiteral("b")),
                                              step(Action::Fixup, QStringLiteral("c")) },
                                            QStringLiteral("/m"));
        QVERIFY(todo.has_value());
        QCOMPARE(todo->text, QStringLiteral("pick a subject a\ndrop b subject b\nfixup c subject c\n"));
    }

    void foldingIntoNothingIsRefused()
    {
        QVERIFY(!rebase::buildTodo({ step(Action::Squash, QStringLiteral("a")) }, QStringLiteral("/m")).has_value());
        QVERIFY(!rebase::buildTodo({ step(Action::Drop, QStringLiteral("a")), step(Action::Fixup, QStringLiteral("b")) },
                                   QStringLiteral("/m")).has_value());
        QVERIFY(!rebase::buildTodo({ step(Action::Drop, QStringLiteral("a")) }, QStringLiteral("/m")).has_value());
    }

    void quotingSurvivesQuotes()
    {
        QCOMPARE(rebase::shellQuote(QStringLiteral("/tmp/it's here")), QStringLiteral("'/tmp/it'\\''s here'"));
    }

    void actionNamesRoundTrip()
    {
        for (const auto action : { Action::Pick, Action::Reword, Action::Edit, Action::Squash, Action::Fixup, Action::Drop })
            QCOMPARE(rebase::parseAction(rebase::actionName(action)), action);
        QVERIFY(!rebase::parseAction(QStringLiteral("merge")).has_value());
    }
};

QTEST_GUILESS_MAIN(RebaseTodoTest)
#include "rebase_todo_test.moc"
