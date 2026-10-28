#include "ui_driver.hpp"

#include "repository_controller.hpp"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QMetaObject>
#include <QTimer>

#include <cstdio>

namespace GitNaga {

UiDriver::UiDriver(const QString &scriptPath, QQuickWindow *window, RepositoryController *repository, QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_repository(repository)
{
    QFile file(scriptPath);
    if (file.open(QIODevice::ReadOnly))
        m_steps = QJsonDocument::fromJson(file.readAll()).array();
    else
        std::fprintf(stderr, "drive: cannot read %s\n", qPrintable(scriptPath));
}

void UiDriver::start()
{
    QTimer::singleShot(200, this, &UiDriver::next);
}

bool UiDriver::settled() const
{
    return !m_repository->loading() && !m_repository->historyLoading() && !m_repository->busy();
}

void UiDriver::click(const QPointF &point, Qt::MouseButton button, bool twice)
{
    const auto send = [&](QEvent::Type type, Qt::MouseButtons buttons) {
        QMouseEvent event(type, point, m_window->mapToGlobal(point), button, buttons, Qt::NoModifier);
        QCoreApplication::sendEvent(m_window, &event);
    };
    QMouseEvent move(QEvent::MouseMove, point, m_window->mapToGlobal(point), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(m_window, &move);
    send(QEvent::MouseButtonPress, button);
    send(QEvent::MouseButtonRelease, Qt::NoButton);
    if (twice) {
        send(QEvent::MouseButtonPress, button);
        send(QEvent::MouseButtonDblClick, button);
        send(QEvent::MouseButtonRelease, Qt::NoButton);
    }
}

void UiDriver::send(QEvent::Type type, const QPointF &point, Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent event(type, point, m_window->mapToGlobal(point), button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(m_window, &event);
}

// A drag moves in small steps, as a hand does, so drag thresholds and
// per-move handlers see every position in between. Without the release the
// drag stays in flight for a screenshot.
void UiDriver::drag(const QPointF &from, const QPointF &to, bool release)
{
    send(QEvent::MouseMove, from, Qt::NoButton, Qt::NoButton);
    send(QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
    constexpr int steps = 16;
    for (int index = 1; index <= steps; ++index) {
        const auto point = from + (to - from) * (static_cast<qreal>(index) / steps);
        send(QEvent::MouseMove, point, Qt::NoButton, Qt::LeftButton);
    }
    if (release)
        send(QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
}

void UiDriver::key(const QString &sequence)
{
    const auto combination = QKeySequence(sequence)[0];
    QKeyEvent press(QEvent::KeyPress, combination.key(), combination.keyboardModifiers());
    QKeyEvent release(QEvent::KeyRelease, combination.key(), combination.keyboardModifiers());
    QCoreApplication::sendEvent(m_window, &press);
    QCoreApplication::sendEvent(m_window, &release);
}

void UiDriver::type(const QString &text)
{
    for (const auto character : text) {
        QKeyEvent press(QEvent::KeyPress, 0, Qt::NoModifier, QString(character));
        QKeyEvent release(QEvent::KeyRelease, 0, Qt::NoModifier, QString(character));
        QCoreApplication::sendEvent(m_window, &press);
        QCoreApplication::sendEvent(m_window, &release);
    }
}

void UiDriver::next()
{
    if (!m_window || !m_repository)
        return;
    if (m_index >= m_steps.size()) {
        QCoreApplication::quit();
        return;
    }
    const auto step = m_steps.at(m_index).toObject();
    int delay = 120;

    if (step.contains(QStringLiteral("idle"))) {
        // Wait for git, then one more beat for the scene to paint.
        if (!settled()) {
            QTimer::singleShot(100, this, &UiDriver::next);
            return;
        }
        delay = 400;
    } else if (step.contains(QStringLiteral("wait"))) {
        delay = step.value(QStringLiteral("wait")).toInt();
    } else if (step.contains(QStringLiteral("open"))) {
        m_repository->openRepositoryPath(step.value(QStringLiteral("open")).toString());
    } else if (step.contains(QStringLiteral("select"))) {
        const auto target = step.value(QStringLiteral("select"));
        if (target.isDouble())
            m_repository->selectCommit(target.toInt());
        else
            m_repository->selectOid(target.toString());
    } else if (step.contains(QStringLiteral("file"))) {
        m_repository->selectFile(step.value(QStringLiteral("file")).toInt());
    } else if (step.contains(QStringLiteral("click")) || step.contains(QStringLiteral("doubleClick"))
               || step.contains(QStringLiteral("rightClick"))) {
        const bool twice = step.contains(QStringLiteral("doubleClick"));
        const bool right = step.contains(QStringLiteral("rightClick"));
        const auto point = step.value(twice ? QStringLiteral("doubleClick")
                                            : right ? QStringLiteral("rightClick") : QStringLiteral("click")).toArray();
        click(QPointF(point.at(0).toDouble(), point.at(1).toDouble()), right ? Qt::RightButton : Qt::LeftButton, twice);
    } else if (step.contains(QStringLiteral("drag"))) {
        const auto points = step.value(QStringLiteral("drag")).toArray();
        drag(QPointF(points.at(0).toDouble(), points.at(1).toDouble()),
             QPointF(points.at(2).toDouble(), points.at(3).toDouble()), step.value(QStringLiteral("release")).toBool(true));
    } else if (step.contains(QStringLiteral("release"))) {
        const auto point = step.value(QStringLiteral("release")).toArray();
        send(QEvent::MouseButtonRelease, QPointF(point.at(0).toDouble(), point.at(1).toDouble()), Qt::LeftButton, Qt::NoButton);
    } else if (step.contains(QStringLiteral("key"))) {
        key(step.value(QStringLiteral("key")).toString());
    } else if (step.contains(QStringLiteral("type"))) {
        type(step.value(QStringLiteral("type")).toString());
    } else if (step.contains(QStringLiteral("call"))) {
        const auto name = step.value(QStringLiteral("call")).toString().toUtf8();
        const auto args = step.value(QStringLiteral("args")).toArray();
        QList<QVariant> values;
        for (const auto &arg : args)
            values.append(arg.toVariant());
        bool ok = false;
        switch (values.size()) {
        case 0: ok = QMetaObject::invokeMethod(m_repository, name.constData()); break;
        case 1: ok = QMetaObject::invokeMethod(m_repository, name.constData(), Q_ARG(QString, values.at(0).toString())); break;
        case 2: ok = QMetaObject::invokeMethod(m_repository, name.constData(), Q_ARG(QString, values.at(0).toString()),
                                               Q_ARG(QString, values.at(1).toString())); break;
        default: break;
        }
        if (!ok)
            std::fprintf(stderr, "drive: call %s failed\n", name.constData());
    } else if (step.contains(QStringLiteral("set"))) {
        m_repository->setProperty(step.value(QStringLiteral("set")).toString().toUtf8(),
                                  step.value(QStringLiteral("value")).toVariant());
    } else if (step.contains(QStringLiteral("print"))) {
        const auto name = step.value(QStringLiteral("print")).toString();
        std::printf("%s=%s\n", qPrintable(name), qPrintable(m_repository->property(name.toUtf8()).toString()));
        std::fflush(stdout);
    } else if (step.contains(QStringLiteral("shot"))) {
        const auto path = step.value(QStringLiteral("shot")).toString();
        if (!m_window->grabWindow().save(path))
            std::fprintf(stderr, "drive: cannot save %s\n", qPrintable(path));
    } else if (step.contains(QStringLiteral("quit"))) {
        QCoreApplication::quit();
        return;
    }
    ++m_index;
    QTimer::singleShot(delay, this, &UiDriver::next);
}

} // namespace GitNaga
