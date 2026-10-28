#include "ui_driver.hpp"

#include "repository_controller.hpp"

#include <QCoreApplication>
#include <QCursor>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QMetaObject>
#include <QTimer>
#include <QWheelEvent>

#include <cstdio>

namespace GitNaga {

namespace {

// "sync.fetch" addresses a member of the repository's `sync` object; a plain
// name addresses the repository itself.
QObject *resolve(QObject *repository, QByteArray *name)
{
    const auto dot = name->indexOf('.');
    if (dot < 0)
        return repository;
    auto *target = repository->property(name->left(dot).constData()).value<QObject *>();
    *name = name->mid(dot + 1);
    return target ? target : repository;
}

} // namespace

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

void UiDriver::click(const QPointF &point, Qt::MouseButton button, bool twice, Qt::KeyboardModifiers modifiers)
{
    const auto send = [&](QEvent::Type type, Qt::MouseButtons buttons) {
        QMouseEvent event(type, point, m_window->mapToGlobal(point), button, buttons, modifiers);
        QCoreApplication::sendEvent(m_window, &event);
    };
    // Menus open at the cursor, which synthetic events do not move.
    QCursor::setPos(m_window->mapToGlobal(point).toPoint());
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

void UiDriver::hover(const QPointF &point)
{
    QMouseEvent move(QEvent::MouseMove, point, m_window->mapToGlobal(point), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(m_window, &move);
}

void UiDriver::wheel(const QPointF &point, int delta)
{
    QWheelEvent event(point, m_window->mapToGlobal(point), {}, QPoint(0, delta), Qt::NoButton, Qt::NoModifier,
                      Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(m_window, &event);
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

QVariant UiDriver::readProperty(const QString &path) const
{
    // "commitCount" reads the repository; "navigation.search.count" walks
    // object properties, so any QML-facing object can be asserted on.
    const auto parts = path.split(QLatin1Char('.'));
    if (parts.size() == 1)
        return m_repository->property(path.toUtf8());
    // "sync.ahead" names a repository part, "window.graphZoom" the window
    // itself; anything else starts at the window's own properties.
    auto steps = parts;
    QObject *object = m_window;
    if (steps.first() == QStringLiteral("window"))
        steps.removeFirst();
    else if (m_repository->property(steps.first().toUtf8()).isValid())
        object = m_repository;
    // Each step is an object property or, like "selectedCommit.subject", a map key.
    QVariant value = QVariant::fromValue(object);
    for (const auto &part : std::as_const(steps)) {
        if (auto *step = value.value<QObject *>())
            value = step->property(part.toUtf8());
        else
            value = value.toMap().value(part);
    }
    return value;
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
    } else if (step.contains(QStringLiteral("until"))) {
        // Poll a property until it reads `equals`, for work the repository's
        // own idle state does not cover (a search, a blame).
        if (readProperty(step.value(QStringLiteral("until")).toString()).toString()
            != step.value(QStringLiteral("equals")).toVariant().toString()) {
            QTimer::singleShot(50, this, &UiDriver::next);
            return;
        }
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
        // As a person picks it: the file's diff opens in the middle too.
        m_repository->selectFile(step.value(QStringLiteral("file")).toInt());
        QMetaObject::invokeMethod(m_window, "openDiff");
    } else if (step.contains(QStringLiteral("click")) || step.contains(QStringLiteral("doubleClick"))
               || step.contains(QStringLiteral("rightClick"))) {
        const bool twice = step.contains(QStringLiteral("doubleClick"));
        const bool right = step.contains(QStringLiteral("rightClick"));
        const auto point = step.value(twice ? QStringLiteral("doubleClick")
                                            : right ? QStringLiteral("rightClick") : QStringLiteral("click")).toArray();
        // "modifiers": "Shift" or "Ctrl" for shift- and control-clicks.
        const auto modifiers = QKeySequence(step.value(QStringLiteral("modifiers")).toString()
                                            + QStringLiteral("+A"))[0].keyboardModifiers();
        click(QPointF(point.at(0).toDouble(), point.at(1).toDouble()), right ? Qt::RightButton : Qt::LeftButton, twice,
              modifiers);
    } else if (step.contains(QStringLiteral("drag"))) {
        const auto points = step.value(QStringLiteral("drag")).toArray();
        drag(QPointF(points.at(0).toDouble(), points.at(1).toDouble()),
             QPointF(points.at(2).toDouble(), points.at(3).toDouble()), step.value(QStringLiteral("release")).toBool(true));
    } else if (step.contains(QStringLiteral("release"))) {
        const auto point = step.value(QStringLiteral("release")).toArray();
        send(QEvent::MouseButtonRelease, QPointF(point.at(0).toDouble(), point.at(1).toDouble()), Qt::LeftButton, Qt::NoButton);
    } else if (step.contains(QStringLiteral("hover"))) {
        const auto point = step.value(QStringLiteral("hover")).toArray();
        hover(QPointF(point.at(0).toDouble(), point.at(1).toDouble()));
    } else if (step.contains(QStringLiteral("wheel"))) {
        const auto args = step.value(QStringLiteral("wheel")).toArray();
        wheel(QPointF(args.at(0).toDouble(), args.at(1).toDouble()), args.at(2).toInt());
    } else if (step.contains(QStringLiteral("key"))) {
        key(step.value(QStringLiteral("key")).toString());
    } else if (step.contains(QStringLiteral("type"))) {
        type(step.value(QStringLiteral("type")).toString());
    } else if (step.contains(QStringLiteral("call"))) {
        auto name = step.value(QStringLiteral("call")).toString().toUtf8();
        auto *target = resolve(m_repository, &name);
        const auto args = step.value(QStringLiteral("args")).toArray();
        QList<QVariant> values;
        for (const auto &arg : args)
            values.append(arg.toVariant());
        bool ok = false;
        switch (values.size()) {
        case 0: ok = QMetaObject::invokeMethod(target, name.constData()); break;
        case 1: ok = QMetaObject::invokeMethod(target, name.constData(), Q_ARG(QString, values.at(0).toString())); break;
        case 2: ok = QMetaObject::invokeMethod(target, name.constData(), Q_ARG(QString, values.at(0).toString()),
                                               Q_ARG(QString, values.at(1).toString())); break;
        default: break;
        }
        if (!ok)
            std::fprintf(stderr, "drive: call %s failed\n", name.constData());
    } else if (step.contains(QStringLiteral("set"))) {
        auto member = step.value(QStringLiteral("set")).toString().toUtf8();
        resolve(m_repository, &member)->setProperty(member.constData(), step.value(QStringLiteral("value")).toVariant());
    } else if (step.contains(QStringLiteral("print"))) {
        const auto name = step.value(QStringLiteral("print")).toString();
        const auto value = readProperty(name);
        // Lists print joined, so a scenario can expect them as one string.
        const auto text = value.typeId() == QMetaType::QStringList ? value.toStringList().join(QLatin1Char(','))
                                                                   : value.toString();
        std::printf("%s=%s\n", qPrintable(name), qPrintable(text));
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
