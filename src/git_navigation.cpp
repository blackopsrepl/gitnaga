#include "git_navigation.hpp"

#include "git_parse.hpp"

#include <QHash>

namespace GitNaga {

using detail::decode;

namespace detail {

namespace {

struct BlameCommit {
    QString author;
    QString authorEmail;
    QDateTime authoredAt;
    QString summary;
};

// "author-mail <ada@x>" → "ada@x"; git wraps the address in angle brackets.
QString unwrapMail(QString mail)
{
    if (mail.startsWith(QLatin1Char('<')) && mail.endsWith(QLatin1Char('>')))
        mail = mail.mid(1, mail.size() - 2);
    return mail;
}

} // namespace

QVector<BlameLine> parseBlamePorcelain(const QByteArray &output)
{
    // Porcelain names a commit's details only the first time it appears, so
    // they are kept by oid and copied onto every later line of that commit.
    QHash<QString, BlameCommit> commits;
    QVector<BlameLine> lines;
    BlameLine current;
    bool inHeader = false;
    for (const auto &raw : output.split('\n')) {
        if (raw.startsWith('\t')) {
            if (!inHeader)
                continue;
            const auto &commit = commits[current.oid];
            current.author = commit.author;
            current.authorEmail = commit.authorEmail;
            current.authoredAt = commit.authoredAt;
            current.summary = commit.summary;
            current.text = decode(raw.mid(1));
            lines.append(current);
            inHeader = false;
            continue;
        }
        if (!inHeader) {
            const auto fields = raw.split(' ');
            if (fields.size() < 3 || fields.at(0).size() < 40)
                continue;
            current = {};
            current.oid = QString::fromLatin1(fields.at(0));
            current.originalLine = fields.at(1).toInt();
            current.line = fields.at(2).toInt();
            inHeader = true;
            continue;
        }
        const auto space = raw.indexOf(' ');
        const auto key = space < 0 ? raw : raw.left(space);
        const auto value = space < 0 ? QString() : decode(raw.mid(space + 1));
        auto &commit = commits[current.oid];
        if (key == "author")
            commit.author = value;
        else if (key == "author-mail")
            commit.authorEmail = unwrapMail(value);
        else if (key == "author-time")
            commit.authoredAt = QDateTime::fromSecsSinceEpoch(value.toLongLong());
        else if (key == "summary")
            commit.summary = value;
    }
    return lines;
}

QVector<FileRevision> parseFileHistory(const QByteArray &output)
{
    QVector<FileRevision> revisions;
    for (const auto &record : output.split('\x1e')) {
        const auto lines = record.split('\n');
        const auto header = lines.value(0).split('\x1f');
        if (header.size() < 4 || header.at(0).isEmpty())
            continue;
        FileRevision revision;
        revision.oid = QString::fromLatin1(header.at(0));
        revision.author = decode(header.at(1));
        revision.authoredAt = QDateTime::fromSecsSinceEpoch(header.at(2).toLongLong());
        revision.subject = decode(header.at(3));
        // --follow names one path per commit: "M\tpath" or "R087\told\tnew".
        for (qsizetype index = 1; index < lines.size(); ++index) {
            const auto fields = lines.at(index).split('\t');
            if (fields.size() < 2 || fields.at(0).isEmpty())
                continue;
            revision.status = QString::fromLatin1(fields.at(0).left(1));
            if (fields.size() >= 3) {
                revision.oldPath = decode(fields.at(1));
                revision.path = decode(fields.at(2));
            } else {
                revision.path = decode(fields.at(1));
            }
            break;
        }
        revisions.append(std::move(revision));
    }
    return revisions;
}

QString shortAge(const QDateTime &when, const QDateTime &now)
{
    const auto seconds = when.secsTo(now);
    if (seconds < 3600)
        return QStringLiteral("now");
    const auto hours = seconds / 3600;
    if (hours < 24)
        return QStringLiteral("%1h").arg(hours);
    const auto days = hours / 24;
    if (days < 14)
        return QStringLiteral("%1d").arg(days);
    if (days < 60)
        return QStringLiteral("%1w").arg(days / 7);
    if (days < 365)
        return QStringLiteral("%1mo").arg(days / 30);
    return QStringLiteral("%1y").arg(days / 365);
}

} // namespace detail

GitResult<QVector<BlameLine>> blameFile(const QString &worktree, const QString &revision, const QString &path,
                                        const CancelToken &cancel)
{
    QStringList arguments{ QStringLiteral("blame"), QStringLiteral("--porcelain") };
    if (!revision.isEmpty())
        arguments.append(revision);
    arguments << QStringLiteral("--") << path;
    RunOptions options;
    options.cancel = cancel;
    auto output = GitClient::run(worktree, arguments, QStringLiteral("blame"), options);
    if (!output)
        return std::unexpected(output.error());
    return detail::parseBlamePorcelain(*output);
}

GitResult<QVector<FileRevision>> fileHistory(const QString &worktree, const QString &revision, const QString &path,
                                             const CancelToken &cancel)
{
    QStringList arguments{ QStringLiteral("-c"), QStringLiteral("core.quotePath=false"), QStringLiteral("log"),
                           QStringLiteral("--follow"), QStringLiteral("-M"), QStringLiteral("--name-status"),
                           QStringLiteral("--no-color"), QStringLiteral("--format=%x1e%H%x1f%an%x1f%at%x1f%s") };
    arguments << (revision.isEmpty() ? QStringLiteral("HEAD") : revision) << QStringLiteral("--") << path;
    RunOptions options;
    options.cancel = cancel;
    auto output = GitClient::run(worktree, arguments, QStringLiteral("file history"), options);
    if (!output)
        return std::unexpected(output.error());
    return detail::parseFileHistory(*output);
}

} // namespace GitNaga
