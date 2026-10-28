#include "git_client.hpp"
#include "git_test_helpers.hpp"
#include "patch_builder.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace GitNaga;

// Each case builds a patch from a real diff, applies it with real git the
// way the application does, and checks what the index or worktree now holds.
class PatchBuilderTest final : public QObject
{
    Q_OBJECT

    QTemporaryDir m_directory;
    QString m_root;

    void git(const QStringList &arguments) { runGit(m_root, arguments); }
    void write(const QString &name, const QByteArray &contents) { writeFile(m_root + QLatin1Char('/') + name, contents); }

    QString indexed(const QString &name) const
    {
        return QString::fromUtf8(GitClient::run(m_root, { QStringLiteral("show"), QLatin1Char(':') + name },
                                                QStringLiteral("read index")).value_or(QByteArray()));
    }

    QString onDisk(const QString &name) const
    {
        QFile file(m_root + QLatin1Char('/') + name);
        return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
    }

    QVector<DiffLine> diff(const QString &path, DiffSide side, const QString &oldPath = {}) const
    {
        auto lines = GitClient::loadDiff(m_root, {}, path, oldPath, side);
        return lines ? *lines : QVector<DiffLine>{};
    }

    // Rows whose text is exactly one of `texts`.
    static QSet<int> rows(const QVector<DiffLine> &lines, const QStringList &texts)
    {
        QSet<int> result;
        for (int row = 0; row < lines.size(); ++row)
            if (texts.contains(lines.at(row).text))
                result.insert(row);
        return result;
    }

    // Build and apply: `cached` targets the index, `reverse` takes lines out.
    bool apply(const QVector<DiffLine> &lines, const QStringList &texts, bool cached, bool reverse)
    {
        const auto patch = patch::build(lines, rows(lines, texts),
                                        reverse ? patch::Direction::Reverse : patch::Direction::Forward);
        if (!patch) {
            qWarning() << patch.error();
            return false;
        }
        QStringList arguments{ QStringLiteral("apply"), QStringLiteral("--recount") };
        if (cached)
            arguments.append(QStringLiteral("--cached"));
        if (reverse)
            arguments.append(QStringLiteral("--reverse"));
        arguments.append(QStringLiteral("-"));
        RunOptions options;
        options.input = *patch;
        const auto result = GitClient::run(m_root, arguments, QStringLiteral("apply"), options);
        if (!result)
            qWarning().noquote() << result.error().message << "\n" << QString::fromUtf8(*patch);
        return result.has_value();
    }

    void commitBase(const QString &name, const QByteArray &contents)
    {
        write(name, contents);
        git({ QStringLiteral("add"), name });
        git({ QStringLiteral("commit"), QStringLiteral("-qm"), QStringLiteral("base") });
    }

private slots:
    void init()
    {
        QVERIFY(m_directory.isValid());
        m_root = m_directory.path() + QStringLiteral("/") + QString::fromUtf8(QTest::currentTestFunction());
        QVERIFY(QDir().mkpath(m_root));
        git({ QStringLiteral("init"), QStringLiteral("-q"), QStringLiteral("-b"), QStringLiteral("main") });
        git({ QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("GitNaga Test") });
        git({ QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@gitnaga.invalid") });
    }

    void stagesAMixedSelection()
    {
        commitBase(QStringLiteral("f"), "a\nb\nc\nd\ne\n");
        write(QStringLiteral("f"), "a\nB\nc\ne\nf\n");
        const auto lines = diff(QStringLiteral("f"), DiffSide::Unstaged);
        QVERIFY(apply(lines, { QStringLiteral("+B"), QStringLiteral("-d") }, true, false));
        QCOMPARE(indexed(QStringLiteral("f")), QStringLiteral("a\nb\nB\nc\ne\n"));
        QCOMPARE(onDisk(QStringLiteral("f")), QStringLiteral("a\nB\nc\ne\nf\n"));
    }

    void unstagesOneAddedLine()
    {
        commitBase(QStringLiteral("f"), "a\nb\nc\nd\ne\n");
        write(QStringLiteral("f"), "a\nB\nc\ne\nf\n");
        git({ QStringLiteral("add"), QStringLiteral("f") });
        const auto lines = diff(QStringLiteral("f"), DiffSide::Staged);
        QVERIFY(apply(lines, { QStringLiteral("+B") }, true, true));
        QCOMPARE(indexed(QStringLiteral("f")), QStringLiteral("a\nc\ne\nf\n"));
    }

    void discardsOneDeletedLine()
    {
        commitBase(QStringLiteral("f"), "a\nb\nc\nd\ne\n");
        write(QStringLiteral("f"), "a\nB\nc\ne\nf\n");
        const auto lines = diff(QStringLiteral("f"), DiffSide::Unstaged);
        QVERIFY(apply(lines, { QStringLiteral("-d") }, false, true));
        QCOMPARE(onDisk(QStringLiteral("f")), QStringLiteral("a\nB\nc\nd\ne\nf\n"));
        QCOMPARE(indexed(QStringLiteral("f")), QStringLiteral("a\nb\nc\nd\ne\n"));
    }

    void keepsTheMissingNewlineRight()
    {
        commitBase(QStringLiteral("f"), "x\ny");
        write(QStringLiteral("f"), "x\ny\nz");
        const auto lines = diff(QStringLiteral("f"), DiffSide::Unstaged);
        // Appending after a last line that had no newline: the old last line
        // gains one, and the new last line still has none.
        QVERIFY(apply(lines, { QStringLiteral("+z") }, true, false));
        QCOMPARE(indexed(QStringLiteral("f")), QStringLiteral("x\ny\nz"));
        git({ QStringLiteral("reset"), QStringLiteral("-q") });
        // Only the newline fix, not the new line.
        QVERIFY(apply(lines, { QStringLiteral("-y"), QStringLiteral("+y") }, true, false));
        QCOMPARE(indexed(QStringLiteral("f")), QStringLiteral("x\ny\n"));
        // Discarding the new line from the worktree keeps the newline fix.
        QVERIFY(apply(diff(QStringLiteral("f"), DiffSide::Unstaged), { QStringLiteral("+z") }, false, true));
        QCOMPARE(onDisk(QStringLiteral("f")), QStringLiteral("x\ny\n"));
    }

    void stagesLinesAtHunkEdges()
    {
        QByteArray base;
        for (int line = 1; line <= 20; ++line)
            base += QByteArray::number(line) + '\n';
        commitBase(QStringLiteral("f"), base);
        auto changed = base;
        changed.replace("20\n", "twenty\n");
        write(QStringLiteral("f"), "zero\nzero.5\n" + changed);
        const auto lines = diff(QStringLiteral("f"), DiffSide::Unstaged);
        // The second line of the first hunk and the last line of the second:
        // the start of the later hunk must account for the earlier one.
        QVERIFY(apply(lines, { QStringLiteral("+zero.5"), QStringLiteral("-20"), QStringLiteral("+twenty") }, true, false));
        QCOMPARE(indexed(QStringLiteral("f")), QString::fromUtf8("zero.5\n" + changed));
        // And back out only the first line of the file.
        const auto staged = diff(QStringLiteral("f"), DiffSide::Staged);
        QVERIFY(apply(staged, { QStringLiteral("+zero.5") }, true, true));
        QCOMPARE(indexed(QStringLiteral("f")), QString::fromUtf8(changed));
    }

    void unstagesPartOfANewFile()
    {
        commitBase(QStringLiteral("keep"), "keep\n");
        write(QStringLiteral("n"), "a\nb\nc\n");
        git({ QStringLiteral("add"), QStringLiteral("n") });
        const auto lines = diff(QStringLiteral("n"), DiffSide::Staged);
        QVERIFY(apply(lines, { QStringLiteral("+b") }, true, true));
        QCOMPARE(indexed(QStringLiteral("n")), QStringLiteral("a\nc\n"));
        // Everything selected takes the whole file back out of the index.
        QVERIFY(apply(diff(QStringLiteral("n"), DiffSide::Staged), { QStringLiteral("+a"), QStringLiteral("+c") }, true, true));
        QVERIFY(!GitClient::run(m_root, { QStringLiteral("cat-file"), QStringLiteral("-e"), QStringLiteral(":n") },
                                QStringLiteral("probe")).has_value());
    }

    void stagesPartOfADeletion()
    {
        commitBase(QStringLiteral("f"), "a\nb\nc\n");
        QFile::remove(m_root + QStringLiteral("/f"));
        const auto lines = diff(QStringLiteral("f"), DiffSide::Unstaged);
        QVERIFY(apply(lines, { QStringLiteral("-b") }, true, false));
        QCOMPARE(indexed(QStringLiteral("f")), QStringLiteral("a\nc\n"));
    }

    void unstagesALineOfARename()
    {
        commitBase(QStringLiteral("old"), "one\ntwo\nthree\nfour\nfive\n");
        git({ QStringLiteral("mv"), QStringLiteral("old"), QStringLiteral("new") });
        write(QStringLiteral("new"), "one\ntwo\nthree\nfour\nfive\nsix\n");
        git({ QStringLiteral("add"), QStringLiteral("new") });
        const auto lines = diff(QStringLiteral("new"), DiffSide::Staged, QStringLiteral("old"));
        QVERIFY(std::ranges::any_of(lines, [](const DiffLine &line) { return line.text.startsWith(QStringLiteral("rename from")); }));
        QVERIFY(apply(lines, { QStringLiteral("+six") }, true, true));
        // The rename stays staged; only the line went.
        QCOMPARE(indexed(QStringLiteral("new")), QStringLiteral("one\ntwo\nthree\nfour\nfive\n"));
        QVERIFY(!GitClient::run(m_root, { QStringLiteral("cat-file"), QStringLiteral("-e"), QStringLiteral(":old") },
                                QStringLiteral("probe")).has_value());
    }

    void refusesWhatCannotBeSplit()
    {
        QVector<DiffLine> conflict{ { DiffLine::Kind::Header, QStringLiteral("diff --cc f") },
                                    { DiffLine::Kind::Hunk, QStringLiteral("@@@ -1,1 -1,1 +1,5 @@@") } };
        QVERIFY(!patch::build(conflict, { 1 }, patch::Direction::Forward).has_value());
        commitBase(QStringLiteral("f"), "a\n");
        write(QStringLiteral("f"), "b\n");
        const auto lines = diff(QStringLiteral("f"), DiffSide::Unstaged);
        QVERIFY(!patch::build(lines, {}, patch::Direction::Forward).has_value());
        QCOMPARE(patch::allChanges(lines).size(), 2);
        QCOMPARE(patch::hunkChanges(lines, static_cast<int>(lines.size()) - 3), patch::allChanges(lines));
    }
};

QTEST_GUILESS_MAIN(PatchBuilderTest)

#include "patch_builder_test.moc"
