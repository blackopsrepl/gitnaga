#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

namespace GitNaga {

struct GraphSegment {
    int fromLane = 0;
    int toLane = 0;
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
    QVector<GraphSegment> segments;
    // Synthetic row representing uncommitted changes rather than a real
    // commit; oid stays empty and painting/inspection branch on this flag.
    bool workInProgress = false;
    QString workSummary;
};

struct WorktreeState {
    int staged = 0;
    int unstaged = 0;
    int untracked = 0;

    int total() const { return staged + unstaged + untracked; }
    bool dirty() const { return total() > 0; }
};

struct RepositorySnapshot {
    QString worktree;
    QString gitDirectory;
    QString commonDirectory;
    QString currentBranch;
    WorktreeState changes;
    QVector<Commit> commits;
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
