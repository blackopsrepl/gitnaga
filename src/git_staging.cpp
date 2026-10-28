#include "git_staging.hpp"

#include "git_parse.hpp"

namespace GitNaga::staging {

using detail::decode;

namespace {

QStringList withPaths(QStringList arguments, const QStringList &paths)
{
    arguments.append(QStringLiteral("--"));
    arguments.append(paths);
    return arguments;
}

GitResult<QString> done(const GitResult<QByteArray> &result)
{
    if (!result)
        return std::unexpected(result.error());
    return decode(*result).trimmed();
}

} // namespace

void splitStatus(const QVector<FileChange> &files, QVector<FileChange> *unstaged, QVector<FileChange> *staged)
{
    for (const auto &file : files) {
        if (file.unstaged) {
            auto entry = file;
            entry.staged = false;
            entry.oldPath.clear();
            if (!file.untracked && !file.conflicted && !file.worktreeStatus.isEmpty())
                entry.status = file.worktreeStatus;
            unstaged->append(std::move(entry));
        }
        if (file.staged) {
            auto entry = file;
            entry.unstaged = false;
            staged->append(std::move(entry));
        }
    }
}

GitResult<QString> stage(const QString &worktree, const QStringList &paths, const RunOptions &options)
{
    // -A so a deleted path stages its deletion instead of failing.
    return done(GitClient::run(worktree, withPaths({ QStringLiteral("add"), QStringLiteral("-A") }, paths),
                               QStringLiteral("stage"), options));
}

GitResult<QString> unstage(const QString &worktree, const QStringList &paths, const RunOptions &options)
{
    // reset, unlike restore --staged, also works on a branch with no commit.
    return done(GitClient::run(worktree, withPaths({ QStringLiteral("reset"), QStringLiteral("-q") }, paths),
                               QStringLiteral("unstage"), options));
}

GitResult<QString> discard(const QString &worktree, const QStringList &tracked, const QStringList &untracked,
                           const RunOptions &options)
{
    if (!tracked.isEmpty()) {
        auto restored = GitClient::run(worktree, withPaths({ QStringLiteral("checkout") }, tracked),
                                       QStringLiteral("discard changes"), options);
        if (!restored)
            return std::unexpected(restored.error());
    }
    if (untracked.isEmpty())
        return QString();
    return done(GitClient::run(worktree,
                               withPaths({ QStringLiteral("clean"), QStringLiteral("-f"), QStringLiteral("-q") },
                                         untracked),
                               QStringLiteral("delete untracked files"), options));
}

GitResult<QString> applyPatch(const QString &worktree, const QByteArray &patch, bool cached, bool reverse,
                              const RunOptions &options)
{
    QStringList arguments{ QStringLiteral("apply"), QStringLiteral("--recount"), QStringLiteral("--whitespace=nowarn") };
    if (cached)
        arguments.append(QStringLiteral("--cached"));
    if (reverse)
        arguments.append(QStringLiteral("--reverse"));
    arguments.append(QStringLiteral("-"));
    auto withInput = options;
    withInput.input = patch;
    return done(GitClient::run(worktree, arguments, QStringLiteral("apply patch"), withInput));
}

GitResult<QString> commit(const QString &worktree, const QString &summary, const QString &description, bool amend,
                          const RunOptions &options)
{
    if (summary.trimmed().isEmpty())
        return std::unexpected(GitError{ QStringLiteral("commit"), QStringLiteral("Empty commit message") });
    QStringList arguments{ QStringLiteral("commit"), QStringLiteral("-m"), summary };
    if (!description.trimmed().isEmpty())
        arguments << QStringLiteral("-m") << description;
    if (amend)
        arguments.append(QStringLiteral("--amend"));
    if (auto committed = GitClient::run(worktree, arguments, QStringLiteral("commit"), options); !committed)
        return std::unexpected(committed.error());
    return done(GitClient::run(worktree, { QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("HEAD") },
                               QStringLiteral("read committed HEAD")));
}

GitResult<HeadMessage> headMessage(const QString &worktree)
{
    auto message = GitClient::run(worktree,
                                  { QStringLiteral("log"), QStringLiteral("-1"), QStringLiteral("--format=%s%x00%b"),
                                    QStringLiteral("HEAD") },
                                  QStringLiteral("read HEAD message"));
    if (!message)
        return std::unexpected(message.error());
    HeadMessage head;
    const auto separator = message->indexOf('\0');
    head.summary = decode(message->left(separator)).trimmed();
    head.description = separator < 0 ? QString() : decode(message->mid(separator + 1)).trimmed();
    // Nothing of HEAD missing from its upstream means HEAD was pushed. No
    // upstream is an error here, and an unpublished commit.
    const auto unpublished = GitClient::run(worktree,
                                            { QStringLiteral("rev-list"), QStringLiteral("-n1"), QStringLiteral("HEAD"),
                                              QStringLiteral("--not"), QStringLiteral("@{upstream}") },
                                            QStringLiteral("compare with upstream"));
    head.published = unpublished && unpublished->trimmed().isEmpty();
    return head;
}

GitResult<QString> stashPush(const QString &worktree, const QString &message, bool includeUntracked,
                             const RunOptions &options)
{
    QStringList arguments{ QStringLiteral("stash"), QStringLiteral("push") };
    if (includeUntracked)
        arguments.append(QStringLiteral("--include-untracked"));
    if (!message.trimmed().isEmpty())
        arguments << QStringLiteral("-m") << message.trimmed();
    return done(GitClient::run(worktree, arguments, QStringLiteral("stash"), options));
}

GitResult<QString> stashAction(const QString &worktree, const QString &action, const QString &selector,
                               const QString &oid, const RunOptions &options)
{
    if (!QStringList{ QStringLiteral("apply"), QStringLiteral("pop"), QStringLiteral("drop") }.contains(action))
        return std::unexpected(GitError{ QStringLiteral("stash"), QStringLiteral("Unknown stash action") });
    const auto current = GitClient::run(worktree, { QStringLiteral("rev-parse"), QStringLiteral("--verify"),
                                                    QStringLiteral("-q"), selector },
                                        QStringLiteral("find stash"));
    if (!current || decode(*current).trimmed() != oid)
        return std::unexpected(GitError{ QStringLiteral("stash"),
                                         QStringLiteral("The stash list changed; %1 is no longer that stash").arg(selector) });
    return done(GitClient::run(worktree, { QStringLiteral("stash"), action, selector },
                               QStringLiteral("stash ") + action, options));
}

GitResult<CommitInspection> inspectStash(const QString &worktree, const QString &oid)
{
    auto inspection = GitClient::inspectCommit(worktree, oid);
    if (!inspection || inspection->details.parents.size() < 3)
        return inspection;
    auto listed = GitClient::run(worktree,
                                 { QStringLiteral("ls-tree"), QStringLiteral("-r"), QStringLiteral("-z"),
                                   QStringLiteral("--name-only"), inspection->details.parents.at(2) },
                                 QStringLiteral("list stashed untracked files"));
    if (!listed)
        return std::unexpected(listed.error());
    for (const auto &path : listed->split('\0')) {
        if (path.isEmpty())
            continue;
        FileChange file;
        file.status = QStringLiteral("A");
        file.path = decode(path);
        file.untracked = true;
        inspection->files.append(std::move(file));
    }
    return inspection;
}

} // namespace GitNaga::staging
