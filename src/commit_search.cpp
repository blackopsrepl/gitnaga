#include "commit_search.hpp"

#include <QRegularExpression>

namespace GitNaga {

namespace {

struct Prefix {
    QLatin1StringView text;
    SearchQuery::Kind kind;
};

constexpr Prefix kPrefixes[] = {
    { QLatin1StringView("path:"), SearchQuery::Kind::Path },
    { QLatin1StringView("content:"), SearchQuery::Kind::Content },
    { QLatin1StringView("author:"), SearchQuery::Kind::Author },
};

// Fewer than four hex digits match a sixteenth of every history; that is
// noise, not a commit someone is looking for.
bool looksLikeOidPrefix(const QString &term)
{
    static const QRegularExpression hex(QStringLiteral("^[0-9a-fA-F]{4,40}$"));
    return hex.match(term).hasMatch();
}

bool hasGlob(const QString &term)
{
    return term.contains(QLatin1Char('*')) || term.contains(QLatin1Char('?')) || term.contains(QLatin1Char('['));
}

} // namespace

SearchQuery parseSearchQuery(const QString &input)
{
    const auto trimmed = input.trimmed();
    for (const auto &prefix : kPrefixes) {
        if (trimmed.startsWith(prefix.text, Qt::CaseInsensitive))
            return { prefix.kind, trimmed.mid(prefix.text.size()).trimmed() };
    }
    return { SearchQuery::Kind::Text, trimmed };
}

QString searchKindName(SearchQuery::Kind kind)
{
    switch (kind) {
    case SearchQuery::Kind::Text: return QStringLiteral("text");
    case SearchQuery::Kind::Author: return QStringLiteral("author");
    case SearchQuery::Kind::Path: return QStringLiteral("path");
    case SearchQuery::Kind::Content: return QStringLiteral("content");
    }
    return {};
}

bool matchesRecord(const SearchRecord &record, const SearchQuery &query)
{
    if (query.term.isEmpty())
        return false;
    const auto &term = query.term;
    const auto contains = [&term](const QString &field) { return field.contains(term, Qt::CaseInsensitive); };
    switch (query.kind) {
    case SearchQuery::Kind::Author:
        return contains(record.author) || contains(record.email);
    case SearchQuery::Kind::Text:
        if (contains(record.subject) || contains(record.author) || contains(record.email))
            return true;
        return looksLikeOidPrefix(term) && record.oid.startsWith(term, Qt::CaseInsensitive);
    case SearchQuery::Kind::Path:
    case SearchQuery::Kind::Content:
        return false;
    }
    return false;
}

QStringList matchRecords(const QVector<SearchRecord> &records, const SearchQuery &query, const CancelToken &cancel)
{
    QStringList oids;
    if (query.needsGit() || query.isEmpty())
        return oids;
    qsizetype checked = 0;
    for (const auto &record : records) {
        // Checking the token is cheap, but not free; every few thousand rows
        // is often enough to drop a superseded query within a millisecond.
        if (cancel && (++checked & 0xfff) == 0 && cancel->load())
            break;
        if (matchesRecord(record, query))
            oids.append(record.oid);
    }
    return oids;
}

GitResult<QStringList> searchHistory(const QString &worktree, const QStringList &revisions, const SearchQuery &query,
                                     const CancelToken &cancel)
{
    if (!query.needsGit() || query.isEmpty() || revisions.isEmpty())
        return QStringList{};
    QStringList arguments{ QStringLiteral("log"), QStringLiteral("--stdin"), QStringLiteral("--no-color"),
                           QStringLiteral("--format=%H") };
    if (query.kind == SearchQuery::Kind::Content)
        arguments.append(QStringLiteral("-S") + query.term);
    arguments.append(QStringLiteral("--"));
    if (query.kind == SearchQuery::Kind::Path)
        // A plain path also matches everything below a directory; a glob has
        // to be marked as one or git compares it literally.
        arguments.append(hasGlob(query.term) ? QStringLiteral(":(glob)") + query.term : query.term);

    RunOptions options;
    options.cancel = cancel;
    options.input = revisions.join(QLatin1Char('\n')).toUtf8() + '\n';
    auto output = GitClient::run(worktree, arguments, QStringLiteral("search history"), options);
    if (!output)
        return std::unexpected(output.error());
    QStringList oids;
    for (const auto &line : output->split('\n')) {
        if (!line.isEmpty())
            oids.append(QString::fromLatin1(line.trimmed()));
    }
    return oids;
}

} // namespace GitNaga
