#include "git_refs.hpp"

#include <QRegularExpression>

#include <algorithm>

namespace GitNaga::gitrefs {

namespace {

const QString headsPrefix = QStringLiteral("refs/heads/");
const QString remotesPrefix = QStringLiteral("refs/remotes/");
const QString tagsPrefix = QStringLiteral("refs/tags/");

} // namespace

bool splitRemoteRef(const QString &name, QString *remote, QString *branch)
{
    const auto separator = name.indexOf(QLatin1Char('/'));
    if (separator <= 0 || separator + 1 >= name.size())
        return false;
    if (remote)
        *remote = name.left(separator);
    if (branch)
        *branch = name.mid(separator + 1);
    return true;
}

QString joinRemoteRef(const QString &remote, const QString &branch)
{
    return remote + QLatin1Char('/') + branch;
}

Ref classify(const QString &fullName)
{
    Ref ref;
    ref.fullName = fullName;
    if (fullName.startsWith(headsPrefix)) {
        ref.kind = RefKind::Local;
        ref.name = fullName.mid(headsPrefix.size());
        ref.branch = ref.name;
    } else if (fullName.startsWith(remotesPrefix)) {
        ref.kind = RefKind::Remote;
        ref.name = fullName.mid(remotesPrefix.size());
        splitRemoteRef(ref.name, &ref.remote, &ref.branch);
        // origin/HEAD only says which branch the server calls default; it is
        // never a branch of its own.
        ref.internal = ref.branch == QStringLiteral("HEAD");
    } else if (fullName.startsWith(tagsPrefix)) {
        ref.kind = RefKind::Tag;
        ref.name = fullName.mid(tagsPrefix.size());
    } else if (fullName == QStringLiteral("refs/stash")) {
        ref.kind = RefKind::Stash;
        ref.name = QStringLiteral("stash");
        ref.internal = true;
    } else {
        ref.kind = RefKind::Other;
        ref.name = fullName.startsWith(QStringLiteral("refs/")) ? fullName.mid(5) : fullName;
        ref.internal = true;
    }
    return ref;
}

QString refFormat()
{
    return QStringLiteral("--format=%(refname)%00%(objectname)%00%(objecttype)%00%(*objectname)%00"
                          "%(*objecttype)%00%(upstream)%00%(upstream:track,nobracket)");
}

void parseTrack(const QString &track, int *ahead, int *behind, bool *gone)
{
    *ahead = 0;
    *behind = 0;
    *gone = track.trimmed() == QStringLiteral("gone");
    static const QRegularExpression aheadExpression(QStringLiteral("ahead (\\d+)"));
    static const QRegularExpression behindExpression(QStringLiteral("behind (\\d+)"));
    if (const auto match = aheadExpression.match(track); match.hasMatch())
        *ahead = match.captured(1).toInt();
    if (const auto match = behindExpression.match(track); match.hasMatch())
        *behind = match.captured(1).toInt();
}

QVector<Ref> parseRefs(const QByteArray &output)
{
    QVector<Ref> refs;
    for (const auto &line : output.split('\n')) {
        const auto fields = line.split('\0');
        if (fields.size() < 7 || fields.at(0).isEmpty())
            continue;
        auto ref = classify(QString::fromUtf8(fields.at(0)));
        const auto objectType = QString::fromUtf8(fields.at(2));
        const auto peeledOid = QString::fromUtf8(fields.at(3));
        const auto peeledType = QString::fromUtf8(fields.at(4));
        // An annotated tag names a tag object; the graph needs the commit it
        // points at. A ref to a tree or a blob has no place in a history.
        if (!peeledOid.isEmpty())
            ref.target = peeledType == QStringLiteral("commit") ? peeledOid : QString();
        else
            ref.target = objectType == QStringLiteral("commit") ? QString::fromUtf8(fields.at(1)) : QString();
        const auto upstream = QString::fromUtf8(fields.at(5));
        if (!upstream.isEmpty())
            ref.upstream = upstream.startsWith(remotesPrefix) ? upstream.mid(remotesPrefix.size()) : upstream;
        parseTrack(QString::fromUtf8(fields.at(6)), &ref.ahead, &ref.behind, &ref.upstreamGone);
        refs.append(std::move(ref));
    }
    return refs;
}

QVector<StashEntry> parseStashes(const QByteArray &output)
{
    QVector<StashEntry> stashes;
    for (const auto &line : output.split('\n')) {
        const auto fields = line.split('\x1f');
        if (fields.size() < 3 || fields.at(0).isEmpty())
            continue;
        stashes.append({ QString::fromUtf8(fields.at(0)), QString::fromUtf8(fields.at(1)),
                         QString::fromUtf8(fields.at(2)) });
    }
    return stashes;
}

QHash<QString, QVector<Ref>> decorations(const QVector<Ref> &refs, bool showInternal)
{
    QHash<QString, QVector<Ref>> byOid;
    for (const auto &ref : refs) {
        if (ref.target.isEmpty() || ref.kind == RefKind::Stash || (ref.internal && !showInternal))
            continue;
        byOid[ref.target].append(ref);
    }
    // The checked-out branch first, then local branches, remotes, tags: the
    // chip a person looks for leads the row.
    for (auto it = byOid.begin(); it != byOid.end(); ++it) {
        std::stable_sort(it->begin(), it->end(), [](const Ref &left, const Ref &right) {
            if (left.isHead != right.isHead)
                return left.isHead;
            return static_cast<int>(left.kind) < static_cast<int>(right.kind);
        });
    }
    return byOid;
}

QString kindName(RefKind kind)
{
    switch (kind) {
    case RefKind::Local: return QStringLiteral("local");
    case RefKind::Remote: return QStringLiteral("remote");
    case RefKind::Tag: return QStringLiteral("tag");
    case RefKind::Stash: return QStringLiteral("stash");
    case RefKind::Other: return QStringLiteral("other");
    }
    return QStringLiteral("other");
}

} // namespace GitNaga::gitrefs
