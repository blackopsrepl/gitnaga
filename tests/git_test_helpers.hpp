#pragma once

#include <QProcess>
#include <QTest>

#include <QString>

// Small git helpers shared by the client-facing test binaries.
inline void runGit(const QString &directory, const QStringList &arguments)
{
    QProcess process;
    process.setWorkingDirectory(directory);
    process.start(QStringLiteral("git"), arguments);
    QVERIFY2(process.waitForFinished(), "git command timed out");
    QCOMPARE(process.exitCode(), 0);
}

inline void writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(contents), contents.size());
}
