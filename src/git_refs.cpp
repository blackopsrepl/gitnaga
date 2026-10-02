#include "git_refs.hpp"

namespace GitNaga::gitrefs {

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

bool githubSlug(const QString &url, QString *slug)
{
    const auto at = url.indexOf(QLatin1String("github.com"));
    if (at < 0)
        return false;
    // The host must end there: a "/" or ":" path separator, or nothing at all.
    // "github.com.evil.example" fails this and is not github.com.
    const auto after = at + 10;
    if (after < url.size() && url.at(after) != QLatin1Char('/') && url.at(after) != QLatin1Char(':'))
        return false;

    auto path = url.mid(after + 1);
    // A pasted URL often carries a trailing slash, and a cloned one a ".git".
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    if (path.endsWith(QLatin1String(".git")))
        path.chop(4);
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);

    // A remote URL is not a browser URL: exactly two path segments, so a
    // sub-page or a deeper path is refused rather than half-read.
    const auto separator = path.indexOf(QLatin1Char('/'));
    if (separator <= 0 || separator + 1 >= path.size())
        return false;
    if (path.indexOf(QLatin1Char('/'), separator + 1) >= 0)
        return false;
    if (slug)
        *slug = path;
    return true;
}

} // namespace GitNaga::gitrefs
