#pragma once

#include <QObject>
#include <QStringList>
#include <QVector>

#include <functional>

namespace GitNaga {

// How to take back one operation GitNaga ran: the git commands that invert
// it, worked out from the repository right before the operation changed it.
struct UndoEntry {
    // What the person did, as the undo button names it ("commit", "delete tag v1").
    QString label;
    // Git argument lists, run in order.
    QVector<QStringList> steps;
    // A state path that must be gone once the operation finished; a rebase
    // that stopped again is not finished, so there is nothing to undo yet.
    QString requireGone;
    bool valid() const { return !steps.isEmpty(); }
};

// Runs on the operation's worker thread, before the operation itself.
using UndoCapture = std::function<UndoEntry(const QString &worktree)>;

namespace undo {
// Put the current branch (or detached HEAD) back where it was with
// `git reset <mode>`: --soft for a commit so its changes return as staged
// work, --keep where the worktree must not be overwritten.
UndoCapture resetHead(const QString &label, const QString &mode);
// Switch back to the branch (or detached commit) HEAD was on, then run `after`.
UndoCapture returnTo(const QString &label, const QVector<QStringList> &after = {});
// The inverse is known before anything runs (delete the branch just created).
UndoCapture fixed(const QString &label, const QVector<QStringList> &steps);
// Recreate a ref about to be deleted at the object it names now; an annotated
// tag gets its tag object back, not just the commit.
UndoCapture recreateRef(const QString &label, const QString &fullName, const QVector<QStringList> &after = {});
UndoCapture restoreStash(const QString &label, const QString &selector, const QString &message);
// Finishing a rebase is undone by returning the branch to its pre-rebase tip.
UndoCapture finishRebase(const QString &label);
// An operation that cannot be taken back (a push, a remote deletion).
UndoCapture never(const QString &label);
} // namespace undo

// The last few undoable operations, newest last (`repository.undoStack`).
// Each record remembers the refs right after its operation; once the refs
// move any other way (a terminal, another tool) the stack is cleared, since
// its inverse commands would no longer mean what they meant.
class UndoStack final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString label READ label NOTIFY changed)
    Q_PROPERTY(QString hint READ hint NOTIFY changed)

public:
    using QObject::QObject;

    bool available() const { return !m_records.isEmpty(); }
    QString label() const;
    QString hint() const;
    const UndoEntry *top() const;
    quint64 topId() const;

    void push(UndoEntry entry, const QString &worktree);
    // The last operation cannot be undone; everything before it is out of reach too.
    void block(const QString &label, const QString &worktree);
    // After the inverse of `id` ran.
    void pop(quint64 id);
    void observe(const QString &worktree, const QByteArray &refs);
    void clear();

signals:
    void changed();

private:
    struct Record {
        UndoEntry entry;
        QByteArray refs;
        quint64 id = 0;
    };

    QVector<Record> m_records;
    QString m_worktree;
    QString m_blocked;
    QByteArray m_blockedRefs;
    quint64 m_nextId = 1;
    // The refs the record on top expects are adopted from the first sync
    // after its operation; until then nothing is compared.
    bool m_awaiting = false;
};

} // namespace GitNaga
