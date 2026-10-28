#include "undo_stack.hpp"

namespace GitNaga {

namespace {
constexpr qsizetype depth = 20;
}

QString UndoStack::label() const
{
    if (!m_records.isEmpty())
        return tr("Undo %1").arg(m_records.constLast().entry.label);
    if (!m_blocked.isEmpty())
        return tr("Can't undo %1").arg(m_blocked);
    return {};
}

QString UndoStack::hint() const
{
    if (!m_records.isEmpty())
        return tr("Take back the last %1 (Ctrl+Z)").arg(m_records.constLast().entry.label);
    if (!m_blocked.isEmpty())
        return tr("%1 changed something outside this repository, so GitNaga cannot take it back").arg(m_blocked);
    return tr("Nothing to undo");
}

const UndoEntry *UndoStack::top() const
{
    return m_records.isEmpty() ? nullptr : &m_records.constLast().entry;
}

quint64 UndoStack::topId() const
{
    return m_records.isEmpty() ? 0 : m_records.constLast().id;
}

void UndoStack::push(UndoEntry entry, const QString &worktree)
{
    if (worktree != m_worktree) {
        m_records.clear();
        m_worktree = worktree;
    }
    m_blocked.clear();
    m_records.append({ std::move(entry), {}, m_nextId++ });
    if (m_records.size() > depth)
        m_records.removeFirst();
    m_awaiting = true;
    emit changed();
}

void UndoStack::block(const QString &label, const QString &worktree)
{
    m_records.clear();
    m_worktree = worktree;
    m_blocked = label;
    m_blockedRefs.clear();
    m_awaiting = true;
    emit changed();
}

void UndoStack::pop(quint64 id)
{
    if (m_records.isEmpty() || m_records.constLast().id != id)
        return;
    m_records.removeLast();
    // The record below expects the refs its own operation left, which the
    // inverse just restored; a mismatch at the next sync clears it.
    m_awaiting = false;
    emit changed();
}

void UndoStack::observe(const QString &worktree, const QByteArray &refs)
{
    if (worktree != m_worktree) {
        m_worktree = worktree;
        clear();
        return;
    }
    if (m_awaiting) {
        m_awaiting = false;
        if (!m_records.isEmpty())
            m_records.last().refs = refs;
        else
            m_blockedRefs = refs;
        return;
    }
    if (!m_records.isEmpty() && m_records.constLast().refs != refs)
        clear();
    else if (m_records.isEmpty() && !m_blocked.isEmpty() && m_blockedRefs != refs)
        clear();
}

void UndoStack::clear()
{
    if (m_records.isEmpty() && m_blocked.isEmpty())
        return;
    m_records.clear();
    m_blocked.clear();
    m_awaiting = false;
    emit changed();
}

} // namespace GitNaga
