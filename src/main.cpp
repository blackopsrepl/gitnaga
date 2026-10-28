#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QString>

#include "diff_blob_provider.hpp"
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

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("diffblob"), new GitNaga::DiffBlobProvider);
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

    // Scripted acceptance runs: see ui_driver.hpp.
    if (const auto script = qEnvironmentVariable("GITNAGA_DRIVE"); !script.isEmpty() && !engine.rootObjects().isEmpty()) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        auto *driver = new GitNaga::UiDriver(script, window, &repository, &application);
        driver->start();
    }

    return application.exec();
}
