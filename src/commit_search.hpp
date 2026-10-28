#pragma once

#include "git_client.hpp"

#include <QString>
#include <QStringList>
#include <QVector>

namespace GitNaga {

// What a person typed into the graph's search field, split into the kind of
// question and its term. Text and author questions are answered from the
// loaded history; path and content questions need git to read the trees.
struct SearchQuery {
    enum class Kind { Text, Author, Path, Content };

    Kind kind = Kind::Text;
    QString term;

    bool needsGit() const { return kind == Kind::Path || kind == Kind::Content; }
    bool isEmpty() const { return term.isEmpty(); }
};

// The fields of one loaded commit a text search looks at. A snapshot of these
// is handed to a worker thread, so matching never reads the live model.
struct SearchRecord {
    QString oid;
    QString subject;
    QString author;
    QString email;
};

SearchQuery parseSearchQuery(const QString &input);
QString searchKindName(SearchQuery::Kind kind);
bool matchesRecord(const SearchRecord &record, const SearchQuery &query);
// Oids of the records that match, in record order. Stops early (and returns
// what it has) once the cancel token is set: a newer query superseded it.
QStringList matchRecords(const QVector<SearchRecord> &records, const SearchQuery &query,
                         const CancelToken &cancel = {});

// Path and content questions over the revisions the graph shows. The answer is
// the set of commits git found, in git's order; the caller maps them to rows.
GitResult<QStringList> searchHistory(const QString &worktree, const QStringList &revisions,
                                     const SearchQuery &query, const CancelToken &cancel = {});

} // namespace GitNaga
