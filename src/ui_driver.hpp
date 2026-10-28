#pragma once

#include <QJsonArray>
#include <QObject>
#include <QPointer>
#include <QQuickWindow>

namespace GitNaga {

class RepositoryController;

// Drives the real window from a JSON script so a release can be looked at the
// same way every time: open a repository, click, type, wait for git to
// settle, save what the window shows. Input goes through Qt's own event
// delivery, exactly as a pointer or keyboard would.
//
// GITNAGA_DRIVE=<script.json> gitnaga
//
// Steps: {"open": path} {"idle": true} {"wait": ms} {"select": row|oid}
// {"file": row} {"click": [x, y], "modifiers": "Shift"} {"doubleClick": [x, y]} {"rightClick": [x, y]}
// {"hover": [x, y]} {"wheel": [x, y, angleDelta]} {"key": "Ctrl+K"} {"type": "text"} {"call": name, "args": [...]}
// {"set": property, "value": v}  (names may be "sync.<member>")
// {"drag": [x1, y1, x2, y2], "release": false} {"release": [x, y]}
// {"shot": path} {"print": property} {"quit": true}
class UiDriver final : public QObject
{
    Q_OBJECT

public:
    UiDriver(const QString &scriptPath, QQuickWindow *window, RepositoryController *repository,
             QObject *parent = nullptr);
    void start();

private:
    void next();
    bool settled() const;
    void click(const QPointF &point, Qt::MouseButton button, bool twice, Qt::KeyboardModifiers modifiers = {});
    void drag(const QPointF &from, const QPointF &to, bool release);
    void send(QEvent::Type type, const QPointF &point, Qt::MouseButton button, Qt::MouseButtons buttons);
    void hover(const QPointF &point);
    void wheel(const QPointF &point, int delta);
    void key(const QString &sequence);
    void type(const QString &text);

    QJsonArray m_steps;
    qsizetype m_index = 0;
    QPointer<QQuickWindow> m_window;
    QPointer<RepositoryController> m_repository;
};

} // namespace GitNaga
