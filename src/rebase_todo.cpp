#include "rebase_todo.hpp"

#include <array>

namespace GitNaga::rebase {

namespace {

constexpr std::array actionNames{ "pick", "reword", "edit", "squash", "fixup", "drop" };

bool folds(Action action)
{
    return action == Action::Squash || action == Action::Fixup;
}

QString line(const QString &command, const Step &step)
{
    // The subject is a comment to git; a stray newline would start a line of
    // its own, so it is flattened.
    auto subject = step.subject;
    subject.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return command + QLatin1Char(' ') + step.oid + QLatin1Char(' ') + subject + QLatin1Char('\n');
}

QString effectiveMessage(const Step &head)
{
    return head.action == Action::Reword && !head.message.trimmed().isEmpty() ? head.message : head.original;
}

// The message a group (a commit plus whatever folds into it) must end up
// with, or empty when git's own result is already right.
QString groupMessage(const Step &head, const QVector<const Step *> &followers)
{
    QStringList squashed{ effectiveMessage(head) };
    QString explicitMessage;
    bool squash = false;
    for (const auto *step : followers) {
        if (step->action != Action::Squash)
            continue;
        squash = true;
        squashed.append(step->original);
        explicitMessage = step->message;
    }
    if (squash)
        return explicitMessage.trimmed().isEmpty() ? combineMessages(squashed) : explicitMessage;
    if (head.action == Action::Reword && head.message.trimmed() != head.original.trimmed())
        return effectiveMessage(head);
    return {};
}

} // namespace

std::optional<Action> parseAction(const QString &name)
{
    for (std::size_t index = 0; index < actionNames.size(); ++index) {
        if (name == QLatin1String(actionNames[index]))
            return static_cast<Action>(index);
    }
    return std::nullopt;
}

QString actionName(Action action)
{
    return QLatin1String(actionNames[static_cast<std::size_t>(action)]);
}

QString shellQuote(const QString &word)
{
    auto quoted = word;
    quoted.replace(QStringLiteral("'"), QStringLiteral("'\\''"));
    return QLatin1Char('\'') + quoted + QLatin1Char('\'');
}

QString combineMessages(const QStringList &messages)
{
    QStringList parts;
    for (const auto &message : messages) {
        if (const auto trimmed = message.trimmed(); !trimmed.isEmpty())
            parts.append(trimmed);
    }
    return parts.join(QStringLiteral("\n\n")) + QLatin1Char('\n');
}

std::expected<Todo, QString> buildTodo(const QVector<Step> &steps, const QString &messageDirectory)
{
    Todo todo;
    bool kept = false;
    for (qsizetype index = 0; index < steps.size();) {
        const auto &head = steps.at(index++);
        if (head.action == Action::Drop) {
            todo.text += line(QStringLiteral("drop"), head);
            continue;
        }
        if (folds(head.action)) {
            return std::unexpected(QStringLiteral("%1 cannot be squashed or fixed up: no kept commit comes before it.")
                                       .arg(head.oid.left(8)));
        }
        kept = true;
        todo.text += line(head.action == Action::Edit ? QStringLiteral("edit") : QStringLiteral("pick"), head);
        // Folding as `fixup` keeps git from opening an editor for the
        // combined message; the message is written by the exec line below.
        QVector<const Step *> followers;
        while (index < steps.size() && (folds(steps.at(index).action) || steps.at(index).action == Action::Drop)) {
            const auto &step = steps.at(index++);
            followers.append(&step);
            todo.text += line(step.action == Action::Drop ? QStringLiteral("drop") : QStringLiteral("fixup"), step);
        }
        const auto message = groupMessage(head, followers);
        if (message.isEmpty())
            continue;
        // Messages reach git as files, not through GIT_EDITOR: an exec line
        // runs again after `rebase --continue` from any later stop, and a file
        // inside the git directory is still there then, where an editor
        // script tied to this one process would not be.
        const auto path = messageDirectory + QStringLiteral("/message-%1").arg(todo.messages.size() + 1);
        todo.messages.append({ path, message });
        todo.text += QStringLiteral("exec git commit --amend --only --no-verify --allow-empty --cleanup=whitespace"
                                    " --quiet -F ")
                   + shellQuote(path) + QLatin1Char('\n');
    }
    if (!kept)
        return std::unexpected(QStringLiteral("Every commit is dropped; keep at least one, or reset the branch instead."));
    return todo;
}

} // namespace GitNaga::rebase
