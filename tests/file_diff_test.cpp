#include "git_client.hpp"
#include "git_test_helpers.hpp"

#include <QBuffer>
#include <QDir>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

using namespace GitNaga;

namespace {

void initRepository(const QString &root)
{
    QDir().mkpath(root);
    runGit(root, { QStringLiteral("init"), QStringLiteral("-q"), QStringLiteral("-b"), QStringLiteral("main") });
    runGit(root, { QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("GitNaga Test") });
    runGit(root, { QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@gitnaga.invalid") });
    runGit(root, { QStringLiteral("config"), QStringLiteral("commit.gpgsign"), QStringLiteral("false") });
}

void commitAll(const QString &root, const QString &message)
{
    runGit(root, { QStringLiteral("add"), QStringLiteral("-A") });
    runGit(root, { QStringLiteral("commit"), QStringLiteral("-qm"), message });
}

QByteArray png(int width, int height, QColor colour)
{
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(colour);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

QString head(const QString &root)
{
    QProcess git;
    git.setWorkingDirectory(root);
    git.start(QStringLiteral("git"), { QStringLiteral("rev-parse"), QStringLiteral("HEAD") });
    git.waitForFinished();
    return QString::fromUtf8(git.readAllStandardOutput()).trimmed();
}

DiffRequest commitRequest(const QString &root, const QString &path)
{
    return { root, head(root), path, {}, DiffSide::Commit, false };
}

} // namespace

class FileDiffTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadsBothImageBlobs()
    {
        QTemporaryDir directory;
        const auto root = directory.path();
        initRepository(root);
        writeFile(root + QStringLiteral("/logo.png"), png(4, 3, Qt::red));
        commitAll(root, QStringLiteral("add logo"));
        writeFile(root + QStringLiteral("/logo.png"), png(8, 6, Qt::blue));
        commitAll(root, QStringLiteral("grow logo"));

        const auto diff = GitClient::loadFileDiff(commitRequest(root, QStringLiteral("logo.png")));
        QVERIFY(diff.has_value());
        QCOMPARE(diff->special.kind, SpecialFile::Kind::Image);
        QVERIFY(diff->special.oldSize > 0 && diff->special.newSize > 0);
        const auto before = GitClient::readBlob(root, diff->special.oldBlob);
        const auto after = GitClient::readBlob(root, diff->special.newBlob);
        QVERIFY(before.has_value() && after.has_value());
        const auto oldImage = QImage::fromData(*before);
        const auto newImage = QImage::fromData(*after);
        QCOMPARE(oldImage.size(), QSize(4, 3));
        QCOMPARE(newImage.size(), QSize(8, 6));
        QCOMPARE(newImage.pixelColor(0, 0), QColor(Qt::blue));

        // Uncommitted: HEAD on the left, the worktree file on the right.
        writeFile(root + QStringLiteral("/logo.png"), png(2, 2, Qt::green));
        const DiffRequest worktree{ root, head(root), QStringLiteral("logo.png"), {}, DiffSide::Worktree, false };
        const auto pending = GitClient::loadFileDiff(worktree);
        QVERIFY(pending.has_value());
        QCOMPARE(pending->special.newBlob.kind, BlobSource::Kind::File);
        QCOMPARE(QImage::fromData(*GitClient::readBlob(root, pending->special.newBlob)).size(), QSize(2, 2));
        QCOMPARE(QImage::fromData(*GitClient::readBlob(root, pending->special.oldBlob)).size(), QSize(8, 6));
    }

    void summarisesAMovedSubmodule()
    {
        QTemporaryDir directory;
        const auto library = directory.filePath(QStringLiteral("library"));
        const auto root = directory.filePath(QStringLiteral("super"));
        initRepository(library);
        writeFile(library + QStringLiteral("/a.txt"), "a\n");
        commitAll(library, QStringLiteral("library start"));
        initRepository(root);
        runGit(root, { QStringLiteral("-c"), QStringLiteral("protocol.file.allow=always"), QStringLiteral("submodule"),
                       QStringLiteral("add"), QStringLiteral("-q"), library, QStringLiteral("lib") });
        commitAll(root, QStringLiteral("add lib"));
        const auto checkout = root + QStringLiteral("/lib");
        runGit(checkout, { QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("GitNaga Test") });
        runGit(checkout, { QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("t@gitnaga.invalid") });
        const auto from = head(checkout);
        writeFile(checkout + QStringLiteral("/b.txt"), "b\n");
        commitAll(checkout, QStringLiteral("teach the library b"));
        const auto to = head(checkout);

        const DiffRequest request{ root, head(root), QStringLiteral("lib"), {}, DiffSide::Worktree, false };
        const auto diff = GitClient::loadFileDiff(request);
        QVERIFY(diff.has_value());
        QCOMPARE(diff->special.kind, SpecialFile::Kind::Submodule);
        QCOMPARE(diff->special.fromCommit, from);
        QCOMPARE(diff->special.toCommit, to);
        QVERIFY(diff->special.checkedOut);
        QCOMPARE(diff->special.commits.size(), 1);
        QVERIFY(diff->special.commits.first().endsWith(QStringLiteral(" teach the library b")));
    }

    void recognisesBinaryLfsAndWhitespaceOnlyChanges()
    {
        QTemporaryDir directory;
        const auto root = directory.path();
        initRepository(root);
        writeFile(root + QStringLiteral("/blob.bin"), QByteArray("\x00\x01\x02", 3));
        writeFile(root + QStringLiteral("/asset.psd"),
                  "version https://git-lfs.github.com/spec/v1\noid sha256:abc123\nsize 2048\n");
        writeFile(root + QStringLiteral("/code.py"), "def f():\n    return 1\n");
        commitAll(root, QStringLiteral("start"));
        writeFile(root + QStringLiteral("/blob.bin"), QByteArray("\x00\x01\x02\x03\x04", 5));
        writeFile(root + QStringLiteral("/asset.psd"),
                  "version https://git-lfs.github.com/spec/v1\noid sha256:def456\nsize 4096\n");
        writeFile(root + QStringLiteral("/code.py"), "def f():\n\treturn  1\n");
        commitAll(root, QStringLiteral("change"));

        const auto binary = GitClient::loadFileDiff(commitRequest(root, QStringLiteral("blob.bin")));
        QCOMPARE(binary->special.kind, SpecialFile::Kind::Binary);
        QCOMPARE(binary->special.oldSize, 3);
        QCOMPARE(binary->special.newSize, 5);

        const auto lfs = GitClient::loadFileDiff(commitRequest(root, QStringLiteral("asset.psd")));
        QCOMPARE(lfs->special.kind, SpecialFile::Kind::Lfs);
        QCOMPARE(lfs->special.lfsOld->oid, QStringLiteral("sha256:abc123"));
        QCOMPARE(lfs->special.lfsNew->size, 4096);

        const auto code = GitClient::loadFileDiff(commitRequest(root, QStringLiteral("code.py")));
        QCOMPARE(code->special.kind, SpecialFile::Kind::None);
        QCOMPARE(code->document.meta.additions, 1);
        const auto ignoring = GitClient::loadFileDiff(commitRequest(root, QStringLiteral("code.py")), { true, 3 });
        QCOMPARE(ignoring->special.kind, SpecialFile::Kind::Empty);
    }

    void widensContextOnRequest()
    {
        QTemporaryDir directory;
        const auto root = directory.path();
        initRepository(root);
        QByteArray text;
        for (int i = 0; i < 40; ++i)
            text += "line " + QByteArray::number(i) + "\n";
        writeFile(root + QStringLiteral("/f.txt"), text);
        commitAll(root, QStringLiteral("start"));
        writeFile(root + QStringLiteral("/f.txt"), text.replace("line 20\n", "line twenty\n"));
        commitAll(root, QStringLiteral("edit"));
        const auto request = commitRequest(root, QStringLiteral("f.txt"));
        QCOMPARE(GitClient::loadFileDiff(request, { false, 3 })->document.rows.size(), 1 + 6 + 2);
        QCOMPARE(GitClient::loadFileDiff(request, { false, 10 })->document.rows.size(), 1 + 20 + 2);
        QCOMPARE(GitClient::loadFileDiff(request, { false, -1 })->document.rows.size(), 1 + 39 + 2);
    }
};

QTEST_MAIN(FileDiffTest)
#include "file_diff_test.moc"
