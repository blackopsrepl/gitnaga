#include "repository_controller.hpp"

#include "git_refs.hpp"
#include "graph_palette.hpp"

#include <QCollator>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QSettings>
#include <QVariantMap>

#include <algorithm>

namespace GitNaga {

namespace {

QString settingsGroup(const QString &worktree)
{
    return QStringLiteral("repositories/")
         + QString::fromLatin1(QCryptographicHash::hash(worktree.toUtf8(), QCryptographicHash::Sha1).toHex());
}

int kindRank(RefKind kind)
{
    switch (kind) {
    case RefKind::Local: return 0;
    case RefKind::Remote: return 1;
    case RefKind::Tag: return 2;
    default: return 3;
    }
}

} // namespace

void RepositoryController::rebuildReferences()
{
    struct Entry {
        Ref ref;
        QVariantMap map;
    };
    QVector<Entry> entries;
    for (const auto &ref : m_repository.refs.refs) {
        if (ref.kind == RefKind::Stash || (ref.internal && !m_view.showInternal))
            continue;
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), ref.name);
        entry.insert(QStringLiteral("fullName"), ref.fullName);
        entry.insert(QStringLiteral("kind"), gitrefs::kindName(ref.kind));
        entry.insert(QStringLiteral("remote"), ref.remote);
        entry.insert(QStringLiteral("branch"), ref.branch);
        entry.insert(QStringLiteral("oid"), ref.target);
        entry.insert(QStringLiteral("shortOid"), ref.target.left(8));
        entry.insert(QStringLiteral("isHead"), ref.isHead);
        entry.insert(QStringLiteral("upstream"), ref.upstream);
        entry.insert(QStringLiteral("ahead"), ref.ahead);
        entry.insert(QStringLiteral("behind"), ref.behind);
        entry.insert(QStringLiteral("upstreamGone"), ref.upstreamGone);
        entry.insert(QStringLiteral("internal"), ref.internal);
        entry.insert(QStringLiteral("hidden"), m_view.hidden.contains(ref.fullName));
        entry.insert(QStringLiteral("solo"), m_view.solo == ref.fullName);
        const auto *commit = m_commits.commitAt(m_commits.rowOf(ref.target));
        entry.insert(QStringLiteral("subject"), commit ? commit->subject : QString());
        entry.insert(QStringLiteral("color"), commit ? graph::laneColor(commit->colorIndex).name() : QStringLiteral("#5b6478"));
        entries.append({ ref, entry });
    }

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::stable_sort(entries.begin(), entries.end(), [&collator](const Entry &left, const Entry &right) {
        const int leftRank = kindRank(left.ref.kind);
        const int rightRank = kindRank(right.ref.kind);
        if (leftRank != rightRank)
            return leftRank < rightRank;
        // Newest version first for tags; alphabetical for everything else.
        if (left.ref.kind == RefKind::Tag)
            return collator.compare(left.ref.name, right.ref.name) > 0;
        return collator.compare(left.ref.name, right.ref.name) < 0;
    });

    m_references.clear();
    for (const auto &entry : entries)
        m_references.append(entry.map);
    emit referencesChanged();
}

namespace {

// git words a stash "WIP on <branch>: <oid> <subject>" or "On <branch>:
// <message>"; the sidebar shows what the person wrote, or the subject.
QString stashTitle(const QString &message)
{
    static const QRegularExpression prefix(QStringLiteral("^(?:WIP on|On) [^:]*: "));
    static const QRegularExpression wip(QStringLiteral("^WIP on [^:]*: [0-9a-f]+ "));
    auto title = message;
    if (const auto match = wip.match(title); match.hasMatch())
        return QStringLiteral("WIP: ") + title.mid(match.capturedLength());
    return title.remove(prefix);
}

} // namespace

QVariantList RepositoryController::stashes() const
{
    QVariantList list;
    for (const auto &stash : m_repository.refs.stashes) {
        QVariantMap entry;
        entry.insert(QStringLiteral("oid"), stash.oid);
        entry.insert(QStringLiteral("shortOid"), stash.oid.left(8));
        entry.insert(QStringLiteral("selector"), stash.selector);
        entry.insert(QStringLiteral("message"), stash.message);
        entry.insert(QStringLiteral("title"), stashTitle(stash.message));
        list.append(entry);
    }
    return list;
}

bool RepositoryController::showInternalRefs() const { return m_view.showInternal; }

void RepositoryController::setShowInternalRefs(bool show)
{
    if (m_view.showInternal == show)
        return;
    m_view.showInternal = show;
    saveViewSettings();
    rebuildReferences();
    loadHistory();
}

QStringList RepositoryController::hiddenRefs() const
{
    auto list = QStringList(m_view.hidden.cbegin(), m_view.hidden.cend());
    list.sort();
    return list;
}

QString RepositoryController::soloRef() const { return m_view.solo; }

void RepositoryController::setRefHidden(const QString &fullName, bool hidden)
{
    if (fullName.isEmpty() || m_view.hidden.contains(fullName) == hidden)
        return;
    if (hidden)
        m_view.hidden.insert(fullName);
    else
        m_view.hidden.remove(fullName);
    saveViewSettings();
    rebuildReferences();
    loadHistory();
}

void RepositoryController::showAllRefs()
{
    if (m_view.hidden.isEmpty())
        return;
    m_view.hidden.clear();
    saveViewSettings();
    rebuildReferences();
    loadHistory();
}

void RepositoryController::setSoloRef(const QString &fullName)
{
    // Soloing the soloed ref again turns solo off.
    m_view.solo = m_view.solo == fullName ? QString() : fullName;
    saveViewSettings();
    rebuildReferences();
    loadHistory();
}

void RepositoryController::loadViewSettings()
{
    QSettings settings;
    settings.beginGroup(settingsGroup(m_repository.location.worktree));
    m_view = {};
    m_view.showInternal = settings.value(QStringLiteral("showInternal"), false).toBool();
    const auto hidden = settings.value(QStringLiteral("hidden")).toStringList();
    m_view.hidden = QSet<QString>(hidden.cbegin(), hidden.cend());
    m_view.solo = settings.value(QStringLiteral("solo")).toString();
}

void RepositoryController::saveViewSettings() const
{
    if (m_repository.location.worktree.isEmpty())
        return;
    QSettings settings;
    settings.beginGroup(settingsGroup(m_repository.location.worktree));
    settings.setValue(QStringLiteral("showInternal"), m_view.showInternal);
    settings.setValue(QStringLiteral("hidden"), hiddenRefs());
    settings.setValue(QStringLiteral("solo"), m_view.solo);
}

QVariantList RepositoryController::worktrees() const
{
    QVariantList entries;
    for (const auto &worktree : m_repository.worktrees) {
        QVariantMap entry;
        entry.insert(QStringLiteral("path"), worktree.path);
        entry.insert(QStringLiteral("head"), worktree.head);
        entry.insert(QStringLiteral("branch"), worktree.branch);
        entry.insert(QStringLiteral("lockReason"), worktree.lockReason);
        entry.insert(QStringLiteral("pruneReason"), worktree.pruneReason);
        entry.insert(QStringLiteral("isMain"), worktree.isMain);
        entry.insert(QStringLiteral("isBare"), worktree.isBare);
        entry.insert(QStringLiteral("isDetached"), worktree.isDetached);
        entry.insert(QStringLiteral("isLocked"), worktree.isLocked);
        entry.insert(QStringLiteral("isPrunable"), worktree.isPrunable);
        entry.insert(QStringLiteral("isCurrent"), worktree.path == m_repository.location.worktree);
        entries.append(entry);
    }
    return entries;
}

} // namespace GitNaga
