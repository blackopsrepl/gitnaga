#include "git_client.hpp"
#include "git_test_helpers.hpp"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTest>

#include <optional>

// Deleting a remote branch is the one operation that leaves git for the
// configured gh CLI. These tests drive the real process path with a stub gh on
// PATH, so the exact command, the exit code, and the failure modes are proven
// without a network call.

using namespace GitNaga;

namespace {

QString writeExecutable(const QString &directory, const QString &name, const QByteArray &contents)
{
    const auto path = QDir(directory).filePath(name);
    writeFile(path, contents);
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                                | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
    return path;
}

// A scratch repository whose "github" remote is a GitHub URL, with no network
// activity anywhere: remote URLs are read from the local config.
QString scratchRepository(const QTemporaryDir &directory)
{
    const auto root = directory.path();
    runGit(root, { QStringLiteral("init"), QStringLiteral("-b"), QStringLiteral("main") });
    runGit(root, { QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("GitNaga Test") });
    runGit(root, { QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@gitnaga.invalid") });
    writeFile(root + QStringLiteral("/readme.txt"), QByteArrayLiteral("hello\n"));
    runGit(root, { QStringLiteral("add"), QStringLiteral("readme.txt") });
    runGit(root, { QStringLiteral("commit"), QStringLiteral("-m"), QStringLiteral("initial") });
    return root;
}

} // namespace

class GitRemoteTest final : public QObject
{
    Q_OBJECT

private slots:
    void resolvesTheSlugFromTheConfiguredRemote()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = scratchRepository(directory);
        runGit(root, { QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("github"),
                       QStringLiteral("https://github.com/blackopsrepl/gitnaga.git") });
        runGit(root, { QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("origin"),
                       QStringLiteral("http://vigilance:3002/blackopsrepl/gitnaga.git") });

        const auto slug = GitClient::githubRepository(root, QStringLiteral("github"));
        QVERIFY(slug.has_value());
        QCOMPARE(*slug, QStringLiteral("blackopsrepl/gitnaga"));

        // A remote on another forge has no GitHub slug, and a remote that does
        // not exist resolves to nothing rather than to an empty slug.
        QVERIFY(!GitClient::githubRepository(root, QStringLiteral("origin")).has_value());
        QVERIFY(!GitClient::githubRepository(root, QStringLiteral("upstream")).has_value());
    }

    void refusesANonGithubRemoteBeforeReachingGh()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = scratchRepository(directory);
        runGit(root, { QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("origin"),
                       QStringLiteral("http://vigilance:3002/blackopsrepl/gitnaga.git") });

        const auto result = GitClient::deleteRemoteBranch(root, QStringLiteral("origin"),
                                                          QStringLiteral("feature"));
        QVERIFY(!result.has_value());
        QVERIFY2(result.error().message.contains(QStringLiteral("not a GitHub repository")),
                 qPrintable(result.error().message));
    }

    void refusesEmptyNames()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = scratchRepository(directory);
        QVERIFY(!GitClient::deleteRemoteBranch(root, QString(), QStringLiteral("feature")).has_value());
        QVERIFY(!GitClient::deleteRemoteBranch(root, QStringLiteral("github"), QString()).has_value());
    }

    void deletesThroughTheConfiguredGh()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = scratchRepository(directory);
        runGit(root, { QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("github"),
                       QStringLiteral("git@github.com:blackopsrepl/gitnaga.git") });

        // A stub gh records its argv and reports success.
        const auto log = QDir(directory.path()).filePath(QStringLiteral("gh.log"));
        const auto stub = QDir(directory.path()).filePath(QStringLiteral("bin"));
        QVERIFY(QDir().mkpath(stub));
        writeExecutable(stub, QStringLiteral("gh"),
                        QByteArrayLiteral("#!/bin/sh\nprintf '%s\\n' \"$@\" > \"$GITNAGA_GH_LOG\"\nexit 0\n"));
        const auto previous = qgetenv("PATH");
        qputenv("PATH", stub.toLocal8Bit() + ':' + previous);
        qputenv("GITNAGA_GH_LOG", log.toLocal8Bit());

        const auto result = GitClient::deleteRemoteBranch(root, QStringLiteral("github"),
                                                          QStringLiteral("feature/nested-name"));

        qputenv("PATH", previous);
        qunsetenv("GITNAGA_GH_LOG");

        if (!result)
            qWarning() << "delete failed:" << result.error().operation << result.error().message
                       << result.error().exitCode;
        QVERIFY(result.has_value());
        QFile recorded(log);
        QVERIFY(recorded.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(recorded.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts),
                 QStringList({ QStringLiteral("api"), QStringLiteral("--method"), QStringLiteral("DELETE"),
                               QStringLiteral("repos/blackopsrepl/gitnaga/git/refs/heads/feature/nested-name") }));
    }

    void surfacesTheGhFailure()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = scratchRepository(directory);
        runGit(root, { QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("github"),
                       QStringLiteral("https://github.com/blackopsrepl/gitnaga.git") });

        const auto stub = QDir(directory.path()).filePath(QStringLiteral("bin"));
        QVERIFY(QDir().mkpath(stub));
        writeExecutable(stub, QStringLiteral("gh"),
                        QByteArrayLiteral("#!/bin/sh\necho 'Reference does not exist (HTTP 422)' >&2\nexit 1\n"));
        const auto previous = qgetenv("PATH");
        qputenv("PATH", stub.toLocal8Bit() + ':' + previous);

        const auto result = GitClient::deleteRemoteBranch(root, QStringLiteral("github"),
                                                          QStringLiteral("feature"));

        qputenv("PATH", previous);

        QVERIFY(!result.has_value());
        QVERIFY2(result.error().message.contains(QStringLiteral("Reference does not exist")),
                 qPrintable(result.error().message));
        QVERIFY(result.error().exitCode != 0);
    }

    void reportsAMissingGh()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto root = scratchRepository(directory);
        runGit(root, { QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("github"),
                       QStringLiteral("https://github.com/blackopsrepl/gitnaga.git") });

        // A PATH that still finds git but holds no gh, so the gh lookup itself
        // is what fails rather than the URL resolution before it.
        const auto stub = QDir(directory.path()).filePath(QStringLiteral("bin"));
        QVERIFY(QDir().mkpath(stub));
        const auto gitPath = QStandardPaths::findExecutable(QStringLiteral("git"));
        QVERIFY(!gitPath.isEmpty());
        QVERIFY(QFile::link(gitPath, QDir(stub).filePath(QStringLiteral("git"))));
        const auto previous = qgetenv("PATH");
        qputenv("PATH", stub.toLocal8Bit());

        const auto result = GitClient::deleteRemoteBranch(root, QStringLiteral("github"),
                                                          QStringLiteral("feature"));

        qputenv("PATH", previous);

        QVERIFY(!result.has_value());
        QVERIFY2(result.error().message.contains(QStringLiteral("not installed")),
                 qPrintable(result.error().message));
    }
};

QTEST_GUILESS_MAIN(GitRemoteTest)

#include "git_remote_test.moc"
