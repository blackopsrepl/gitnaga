#pragma once

#include "git_client.hpp"

#include <QDateTime>
#include <QString>
#include <QVector>

namespace GitNaga {

// One line of a file as `git blame` attributes it. The commit fields repeat
// for every line of a commit; QString sharing keeps that cheap.
struct BlameLine {
    QString oid;
    QString author;
    QString authorEmail;
    QDateTime authoredAt;
    QString summary;
    int line = 0;
    int originalLine = 0;
    QString text;

    // All zeros: the line is in the worktree but in no commit yet.
    bool uncommitted() const { return !oid.isEmpty() && oid.count(QLatin1Char('0')) == oid.size(); }
};

// One commit in a file's history, with the name the file had in it, so a
// rename shows where the file came from.
struct FileRevision {
    QString oid;
    QString author;
    QDateTime authoredAt;
    QString subject;
    QString status;
    QString path;
    QString oldPath;
};

namespace detail {
QVector<BlameLine> parseBlamePorcelain(const QByteArray &output);
QVector<FileRevision> parseFileHistory(const QByteArray &output);
// "3y", "5mo", "2w", "4d", "6h", "now": short enough for a blame gutter.
QString shortAge(const QDateTime &when, const QDateTime &now);
} // namespace detail

// Blames `path` as of `revision`, or as the worktree has it when the revision
// is empty: uncommitted lines then come back with an all-zero oid.
GitResult<QVector<BlameLine>> blameFile(const QString &worktree, const QString &revision, const QString &path,
                                        const CancelToken &cancel = {});
// Every commit reachable from `revision` (HEAD when empty) that touched the
// file, following it across renames.
GitResult<QVector<FileRevision>> fileHistory(const QString &worktree, const QString &revision, const QString &path,
                                             const CancelToken &cancel = {});

} // namespace GitNaga
