#pragma once

#include "domain.hpp"

#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

namespace GitNaga::gitrefs {

// Naming of remote-tracking refs. A remote-tracking ref is stored as
// refs/remotes/<remote>/<branch>, so its short name — what the graph and the
// references sidebar show — is "<remote>/<branch>". Git forbids a slash in a
// remote name, so the first segment is always the remote and everything after
// it is the branch, which may itself contain slashes.
//
// Returns false and leaves the outputs untouched when the name carries no
// remote and branch at all.
bool splitRemoteRef(const QString &name, QString *remote, QString *branch);

// The short name of a remote-tracking ref, "<remote>/<branch>", in the form
// the graph and the references sidebar show it.
QString joinRemoteRef(const QString &remote, const QString &branch);

// Kind, display name and visibility of a full ref name. Branches, remote
// branches and tags are what a person browses; a remote's symbolic HEAD, the
// stash ref and every other namespace (tool checkpoints, filter-branch
// backups, notes, prefetch copies) are internal.
Ref classify(const QString &fullName);

// The `for-each-ref` format parseRefs() reads.
QString refFormat();
QVector<Ref> parseRefs(const QByteArray &output);

// "ahead 1, behind 2", "behind 3", "gone" or empty, as
// %(upstream:track,nobracket) prints it.
void parseTrack(const QString &track, int *ahead, int *behind, bool *gone);

QVector<StashEntry> parseStashes(const QByteArray &output);

// Refs each commit wears in the graph, keyed by the peeled commit oid. A
// hidden ref wears no chip, except the checked-out branch, which the graph
// never hides from the person working on it.
QHash<QString, QVector<Ref>> decorations(const QVector<Ref> &refs, bool showInternal,
                                         const QSet<QString> &hidden = {});

QString kindName(RefKind kind);

} // namespace GitNaga::gitrefs
