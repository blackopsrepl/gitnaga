#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QString>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("GitNaga"));
    QCoreApplication::setApplicationVersion(QStringLiteral(GITNAGA_VERSION));
    QCoreApplication::setOrganizationName(QStringLiteral("GitNaga"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("gitnaga.io"));

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &application,
        [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("GitNaga"), QStringLiteral("Main"));

    return application.exec();
}
