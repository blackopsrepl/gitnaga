#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QString>

#include "navigation_controller.hpp"
#include "repository_controller.hpp"
#include "ui_driver.hpp"

#include <QQuickWindow>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("GitNaga"));
    QCoreApplication::setApplicationVersion(QStringLiteral(GITNAGA_VERSION));
    QCoreApplication::setOrganizationName(QStringLiteral("GitNaga"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("gitnaga.io"));

    // The controllers outlive the engine: a scene torn down after them would
    // re-evaluate every binding against a dead object on the way out.
    GitNaga::RepositoryController repository;
    GitNaga::NavigationController navigation(&repository);
    QQmlApplicationEngine engine;
    engine.setInitialProperties({ { QStringLiteral("repository"), QVariant::fromValue(&repository) },
                                  { QStringLiteral("navigation"), QVariant::fromValue(&navigation) } });
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

    // Scripted acceptance runs: see ui_driver.hpp.
    if (const auto script = qEnvironmentVariable("GITNAGA_DRIVE"); !script.isEmpty() && !engine.rootObjects().isEmpty()) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        auto *driver = new GitNaga::UiDriver(script, window, &repository, &application);
        driver->start();
    }

    return application.exec();
}
