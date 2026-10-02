#pragma once

#include <QString>

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

} // namespace GitNaga::gitrefs
