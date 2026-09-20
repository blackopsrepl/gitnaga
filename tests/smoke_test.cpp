#include <QTest>

class SmokeTest final : public QObject
{
    Q_OBJECT

private slots:
    void applicationMetadataContract()
    {
        QVERIFY(QT_VERSION >= QT_VERSION_CHECK(6, 9, 1));
        QCOMPARE(__cplusplus, 202302L);
    }
};

QTEST_GUILESS_MAIN(SmokeTest)

#include "smoke_test.moc"
