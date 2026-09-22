#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

namespace GitNaga {

struct GraphSegment {
    int fromLane = 0;
    int toLane = 0;
    int fromColor = 0;
    int toColor = 0;
};

struct Commit {
    QString oid;
    QStringList parents;
    QString author;
    QString authorEmail;
    QDateTime authoredAt;
    QString subject;
    QStringList refs;
    int lane = 0;
    int laneCount = 1;
    // Colour of the branch line this commit sits on. Unlike the lane, the
    // colour travels with the line as lanes shift, so a branch keeps one
    // colour for its whole visible run.
    int colorIndex = 0;
    QVector<GraphSegment> segments;
};

struct RepositorySnapshot {
    QString worktree;
    QString gitDirectory;
    QString commonDirectory;
    QString currentBranch;
    QVector<Commit> commits;
    // Oid of the ephemeral commit describing uncommitted work, empty when the
    // worktree matches HEAD. The commit is a throwaway object: it is never
    // referenced, and git's own auto-gc reclaims it.
    QString workInProgressOid;
};

struct CommitDetails {
    QString oid;
    QString author;
    QString authorEmail;
    QDateTime authoredAt;
    QString subject;
    QString body;
    QStringList parents;
};

struct FileChange {
    QString status;
    QString path;
    QString oldPath;
    // Meaningful only for the uncommitted-work listing: whether the index
    // already holds this change, which is what a commit would pick up.
    bool staged = false;
};

struct DiffLine {
    enum class Kind { Context, Addition, Deletion, Header, Hunk };

    Kind kind = Kind::Context;
    QString text;
    int oldLine = 0;
    int newLine = 0;
};

struct CommitInspection {
    CommitDetails details;
    QVector<FileChange> files;
};

} // namespace GitNaga
