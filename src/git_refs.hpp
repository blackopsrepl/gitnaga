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

// The "owner/repository" slug inside a GitHub remote URL, in the form the gh
// CLI takes for `gh api repos/<slug>/...`. Accepts the https, git, ssh, and
// scp-style forms of a remote URL, with or without a ".git" suffix or a
// trailing slash, and refuses anything that is not a GitHub repository URL —
// including a host that merely contains "github.com" as a prefix, so a lookalike
// never resolves to someone else's repository.
//
// A remote URL is not a browser URL: exactly two path segments are accepted.
bool githubSlug(const QString &url, QString *slug);

} // namespace GitNaga::gitrefs
