#pragma once

#include "git_test_helpers.hpp"
#include "repository_controller.hpp"

#include <QFile>
#include <QProcess>
#include <QTest>

// Repositories and waits shared by the operation and undo tests.
namespace OperationFixtures {

using namespace GitNaga;

inline QString git(const QString &root, const QStringList &arguments)
{
    QProcess process;
    process.setWorkingDirectory(root);
    process.start(QStringLiteral("git"), arguments);
    process.waitForFinished();
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

inline void commitFile(const QString &root, const QByteArray &contents, const QString &message)
{
    writeFile(root + QStringLiteral("/f.txt"), contents);
    runGit(root, { QStringLiteral("add"), QStringLiteral("f.txt") });
    runGit(root, { QStringLiteral("commit"), QStringLiteral("-qm"), message });
}

// main and other both rewrote f.txt; main is checked out.
inline void makeDivergent(const QString &root)
{
    runGit(root, { QStringLiteral("init"), QStringLiteral("-q"), QStringLiteral("-b"), QStringLiteral("main") });
    runGit(root, { QStringLiteral("config"), QStringLiteral("user.name"), QStringLiteral("GitNaga Test") });
    runGit(root, { QStringLiteral("config"), QStringLiteral("user.email"), QStringLiteral("test@gitnaga.invalid") });
    commitFile(root, "base\n", QStringLiteral("base"));
    runGit(root, { QStringLiteral("switch"), QStringLiteral("-qc"), QStringLiteral("other") });
    commitFile(root, "theirs\n", QStringLiteral("theirs"));
    runGit(root, { QStringLiteral("switch"), QStringLiteral("-q"), QStringLiteral("main") });
    commitFile(root, "ours\n", QStringLiteral("ours"));
}

inline void settle(RepositoryController &controller)
{
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy() && !controller.loading() && !controller.historyLoading(), 10000);
    QTest::qWait(200);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.busy() && !controller.loading(), 10000);
}

inline void open(RepositoryController &controller, const QString &root)
{
    controller.openRepositoryPath(root);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.headOid().isEmpty(), 5000);
    settle(controller);
}

inline QString contents(const QString &root)
{
    QFile file(root + QStringLiteral("/f.txt"));
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

} // namespace OperationFixtures
