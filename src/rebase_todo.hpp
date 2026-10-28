#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <expected>
#include <optional>

namespace GitNaga::rebase {

enum class Action { Pick, Reword, Edit, Squash, Fixup, Drop };

std::optional<Action> parseAction(const QString &name);
QString actionName(Action action);

// One commit of an interactive rebase as the person arranged it.
struct Step {
    Action action = Action::Pick;
    QString oid;
    QString subject;
    // The commit's message as it stands today.
    QString original;
    // Reword: the new message. Squash: the whole message the group ends up
    // with, read from the group's last squash; empty means the default
    // combination of the group's messages.
    QString message;
};

// A message an exec line of the todo reads with `git commit -F`.
struct MessageFile {
    QString path;
    QString text;
};

struct Todo {
    QString text;
    QVector<MessageFile> messages;
};

// Single-quotes a word for /bin/sh, which is what runs exec lines and the
// sequence editor.
QString shellQuote(const QString &word);
// Squash's default message: every message trimmed, one blank line apart.
QString combineMessages(const QStringList &messages);
// The todo git runs, plus the message files its exec lines read; message
// files are named inside `messageDirectory`. Fails with a sentence a person
// can act on when the arrangement cannot be expressed.
std::expected<Todo, QString> buildTodo(const QVector<Step> &steps, const QString &messageDirectory);

} // namespace GitNaga::rebase
