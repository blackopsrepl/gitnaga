#include "undo_stack.hpp"

#include "git_client.hpp"

#include <QDir>
#include <QFile>

namespace GitNaga::undo {

namespace {

// Reads that may legitimately find nothing (an unborn HEAD, a detached one)
// answer with an empty string instead of an error.
QString probe(const QString &worktree, const QStringList &arguments)
{
    RunOptions options;
    options.acceptedExitCodes = { 0, 1, 128 };
    auto output = GitClient::run(worktree, arguments, QStringLiteral("read undo state"), options);
    return output ? QString::fromUtf8(*output).trimmed() : QString();
}

QString headOid(const QString &worktree)
{
    return probe(worktree, { QStringLiteral("rev-parse"), QStringLiteral("-q"), QStringLiteral("--verify"),
                             QStringLiteral("HEAD^{commit}") });
}

QString headBranch(const QString &worktree)
{
    return probe(worktree, { QStringLiteral("symbolic-ref"), QStringLiteral("-q"), QStringLiteral("--short"),
                             QStringLiteral("HEAD") });
}

QString gitPath(const QString &worktree, const QString &path)
{
    const auto relative = probe(worktree, { QStringLiteral("rev-parse"), QStringLiteral("--git-path"), path });
    return relative.isEmpty() ? QString() : QDir(worktree).absoluteFilePath(relative);
}

QString readFirstLine(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readLine()).trimmed();
}

} // namespace

UndoCapture resetHead(const QString &label, const QString &mode)
{
    return [label, mode](const QString &worktree) {
        UndoEntry entry{ label, {}, {} };
        const auto oid = headOid(worktree);
        if (!oid.isEmpty())
            entry.steps.append({ QStringLiteral("reset"), mode, oid });
        return entry;
    };
}

UndoCapture returnTo(const QString &label, const QVector<QStringList> &after)
{
    return [label, after](const QString &worktree) {
        UndoEntry entry{ label, {}, {} };
        const auto branch = headBranch(worktree);
        const auto oid = headOid(worktree);
        if (!branch.isEmpty())
            entry.steps.append({ QStringLiteral("checkout"), branch });
        else if (!oid.isEmpty())
            entry.steps.append({ QStringLiteral("checkout"), QStringLiteral("--detach"), oid });
        else
            return entry;
        entry.steps.append(after);
        return entry;
    };
}

UndoCapture fixed(const QString &label, const QVector<QStringList> &steps)
{
    return [label, steps](const QString &) { return UndoEntry{ label, steps, {} }; };
}

UndoCapture recreateRef(const QString &label, const QString &fullName, const QVector<QStringList> &after)
{
    return [label, fullName, after](const QString &worktree) {
        UndoEntry entry{ label, {}, {} };
        // No ^{} peel: for an annotated tag this is the tag object itself.
        const auto oid = probe(worktree, { QStringLiteral("rev-parse"), QStringLiteral("-q"), QStringLiteral("--verify"),
                                           fullName });
        if (oid.isEmpty())
            return entry;
        // The empty old value makes git refuse if the name was reused meanwhile.
        entry.steps.append({ QStringLiteral("update-ref"), QStringLiteral("-m"), QStringLiteral("gitnaga: undo ") + label,
                             fullName, oid, QString() });
        entry.steps.append(after);
        return entry;
    };
}

UndoCapture restoreStash(const QString &label, const QString &selector, const QString &message)
{
    return [label, selector, message](const QString &worktree) {
        UndoEntry entry{ label, {}, {} };
        const auto oid = probe(worktree, { QStringLiteral("rev-parse"), QStringLiteral("-q"), QStringLiteral("--verify"),
                                           selector });
        if (!oid.isEmpty())
            entry.steps.append({ QStringLiteral("stash"), QStringLiteral("store"), QStringLiteral("-m"), message, oid });
        return entry;
    };
}

UndoCapture finishRebase(const QString &label)
{
    return [label](const QString &worktree) {
        UndoEntry entry{ label, {}, {} };
        for (const auto &state : { QStringLiteral("rebase-merge"), QStringLiteral("rebase-apply") }) {
            const auto directory = gitPath(worktree, state);
            const auto oid = readFirstLine(directory + QStringLiteral("/orig-head"));
            if (oid.isEmpty())
                continue;
            entry.steps.append({ QStringLiteral("reset"), QStringLiteral("--keep"), oid });
            entry.requireGone = directory;
            break;
        }
        return entry;
    };
}

UndoCapture never(const QString &label)
{
    return [label](const QString &) { return UndoEntry{ label, {}, {} }; };
}

} // namespace GitNaga::undo
