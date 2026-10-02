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

} // namespace GitNaga::gitrefs
