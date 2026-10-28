#include "askpass.hpp"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>

#include <cstdio>
#include <cstring>

namespace GitNaga::askpass {
namespace {

// The prompt, or null when this process is not answering one.
const char *promptArgument(int argc, char *argv[])
{
    if (argc >= 2 && std::strcmp(argv[1], "--askpass") == 0)
        return argc >= 3 ? argv[2] : "";
    if (qEnvironmentVariable("GITNAGA_ASKPASS") == QStringLiteral("1"))
        return argc >= 2 ? argv[1] : "";
    return nullptr;
}

void typeInto(QQuickWindow *window, const QString &text)
{
    for (const auto character : text) {
        QKeyEvent press(QEvent::KeyPress, 0, Qt::NoModifier, QString(character));
        QKeyEvent release(QEvent::KeyRelease, 0, Qt::NoModifier, QString(character));
        QCoreApplication::sendEvent(window, &press);
        QCoreApplication::sendEvent(window, &release);
    }
}

void press(QQuickWindow *window, Qt::Key key)
{
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
    QCoreApplication::sendEvent(window, &press);
    QCoreApplication::sendEvent(window, &release);
}

// Scripted answer for headless verification, through the same key delivery
// a keyboard uses, so the dialog's own field and Enter handling are proven.
void scriptAnswer(QQuickWindow *window)
{
    const auto text = qEnvironmentVariable("GITNAGA_ASKPASS_TYPE");
    const bool cancel = qEnvironmentVariableIsSet("GITNAGA_ASKPASS_CANCEL");
    if (text.isEmpty() && !cancel && !qEnvironmentVariableIsSet("GITNAGA_ASKPASS_SHOT"))
        return;
    QTimer::singleShot(300, window, [window, text, cancel] {
        typeInto(window, text);
        QTimer::singleShot(200, window, [window, cancel] {
            if (const auto shot = qEnvironmentVariable("GITNAGA_ASKPASS_SHOT"); !shot.isEmpty())
                window->grabWindow().save(shot);
            if (cancel)
                press(window, Qt::Key_Escape);
            else if (qEnvironmentVariableIsSet("GITNAGA_ASKPASS_TYPE"))
                press(window, Qt::Key_Return);
        });
    });
}

} // namespace

int run(int argc, char *argv[])
{
    const char *prompt = promptArgument(argc, argv);
    if (!prompt)
        return -1;

    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("GitNaga"));
    QCoreApplication::setOrganizationName(QStringLiteral("GitNaga"));
    QQmlApplicationEngine engine;
    engine.setInitialProperties({ { QStringLiteral("prompt"), QString::fromLocal8Bit(prompt) } });
    engine.loadFromModule(QStringLiteral("GitNaga"), QStringLiteral("Askpass"));
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (!window)
        return 1;
    // The dialog says it is done (answered, cancelled or closed) by hiding.
    QObject::connect(window, &QWindow::visibleChanged, &application, [](bool visible) {
        if (!visible)
            QCoreApplication::quit();
    });
    scriptAnswer(window);
    application.exec();
    if (!window->property("accepted").toBool())
        return 1;
    // One line on stdout is the whole protocol git and ssh read.
    std::fputs(window->property("answer").toString().toUtf8().constData(), stdout);
    std::fputs("\n", stdout);
    std::fflush(stdout);
    return 0;
}

} // namespace GitNaga::askpass
