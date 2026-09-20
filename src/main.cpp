#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QString>

#include "repository_controller.hpp"

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("GitNaga"));
    QCoreApplication::setApplicationVersion(QStringLiteral(GITNAGA_VERSION));
    QCoreApplication::setOrganizationName(QStringLiteral("GitNaga"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("gitnaga.io"));

    QQmlApplicationEngine engine;
    GitNaga::RepositoryController repository;
    engine.setInitialProperties({ { QStringLiteral("repository"), QVariant::fromValue(&repository) } });
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &application,
        [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("GitNaga"), QStringLiteral("Main"));

    const auto arguments = application.arguments();
    if (arguments.size() > 1)
        repository.openRepositoryPath(arguments.at(1));

    return application.exec();
}
