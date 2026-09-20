#include <QTest>

class SmokeTest final : public QObject
{
    Q_OBJECT

private slots:
    void applicationMetadataContract()
    {
        QVERIFY(QT_VERSION >= QT_VERSION_CHECK(6, 9, 1));
        // Assert the C++23 language mode, not one compiler's macro value: GCC
        // reports 202100 for -std=c++23 up to version 13 and 202302 from 14 on,
        // while C++20 reports 202002, so the threshold still separates the two.
        QVERIFY2(__cplusplus >= 202100L, "build must use the C++23 language mode");
    }
};

QTEST_GUILESS_MAIN(SmokeTest)

#include "smoke_test.moc"
