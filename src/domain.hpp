#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

namespace GitNaga {

// The row that stands for uncommitted work. It is not an object in the
// repository: nothing is written to show it, and every surface that meets this
// id asks the worktree instead of the object database.
inline QString worktreeOid() { return QStringLiteral("WORKTREE"); }

enum class RefKind { Local, Remote, Tag, Stash, Other };

// One reference as git stores it, already classified. `target` is the commit
// the ref finally names (annotated tags are peeled) and is empty when the ref
// names something that is not a commit.
struct Ref {
    RefKind kind = RefKind::Other;
    QString fullName;
    QString name;
    QString remote;
    QString branch;
    QString target;
    QString upstream;
    int ahead = 0;
    int behind = 0;
    bool upstreamGone = false;
    // A tool's private namespace or a symbolic remote HEAD: real, but not
    // something a person browsing branches asked to see.
    bool internal = false;
    bool isHead = false;
};

struct StashEntry {
    QString oid;
    QString selector;
    QString message;
};

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
    QVector<Ref> refs;
    int lane = 0;
    int laneCount = 1;
    // Colour of the branch line this commit sits on. Unlike the lane, the
    // colour travels with the line as lanes shift, so a branch keeps one
    // colour for its whole visible run.
    int colorIndex = 0;
    QVector<GraphSegment> segments;
};

struct Worktree {
    QString path;
    QString head;
    QString branch;
    QString lockReason;
    QString pruneReason;
    bool isMain = false;
    bool isBare = false;
    bool isDetached = false;
    bool isLocked = false;
    bool isPrunable = false;
};

// Where a repository lives. Discovered once per open, not on every refresh.
struct RepositoryLocation {
    QString worktree;
    QString gitDirectory;
    QString commonDirectory;
    bool isBare = false;
};

// Everything a refresh reads from the refs. It is cheap, and its fingerprint
// decides whether the history has to be walked again at all.
struct RefSnapshot {
    QVector<Ref> refs;
    QVector<StashEntry> stashes;
    QString headOid;
    QString currentBranch;
    bool detached = false;
    QByteArray fingerprint;
    // What only this repository's own operations move: local branches (with
    // their upstream setting), tags, stashes and HEAD. A fetch moves
    // remote-tracking refs and must not look like someone else's change.
    QByteArray localFingerprint;
};

struct FileChange {
    QString status;
    QString path;
    QString oldPath;
    // Uncommitted-work listing only: whether the index holds a change for this
    // path, whether the worktree holds one on top of the index, and whether
    // git tracks the path at all.
    bool staged = false;
    bool unstaged = false;
    // The worktree's own status letter when it differs from the index's.
    QString worktreeStatus;
    bool untracked = false;
    bool conflicted = false;
    bool submodule = false;
};

// The worktree as `git status` sees it, plus any operation git left half done.
struct WorktreeStatus {
    QVector<FileChange> files;
    QString upstream;
    int ahead = 0;
    int behind = 0;
    // "merge", "rebase", "cherry-pick", "revert", "bisect", "am" or empty.
    QString operation;
    QByteArray fingerprint;
};

// One complete read of a repository. The application assembles this
// piecemeal (refs, status and a streamed history); tests read it in one go.
struct RepositorySnapshot {
    RepositoryLocation location;
    RefSnapshot refs;
    WorktreeStatus status;
    QVector<Commit> commits;
    QVector<Worktree> worktrees;
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

struct DiffLine {
    enum class Kind { Context, Addition, Deletion, Header, Hunk };

    Kind kind = Kind::Context;
    QString text;
    int oldLine = 0;
    int newLine = 0;
};

// Which pair of trees a file diff compares.
enum class DiffSide {
    Commit,   // the commit against its first parent
    Worktree, // the worktree against HEAD: everything uncommitted
    Unstaged, // the worktree against the index
    Staged,   // the index against HEAD
};

struct CommitInspection {
    CommitDetails details;
    QVector<FileChange> files;
};

} // namespace GitNaga

// Rows move by memcpy when a long history's storage grows, instead of one
// move constructor per commit.
Q_DECLARE_TYPEINFO(GitNaga::GraphSegment, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(GitNaga::Ref, Q_RELOCATABLE_TYPE);
Q_DECLARE_TYPEINFO(GitNaga::Commit, Q_RELOCATABLE_TYPE);
