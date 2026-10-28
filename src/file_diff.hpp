#pragma once

#include "diff_document.hpp"

#include <optional>

namespace GitNaga {

// One file diff to load, named the way the inspector selects it.
struct DiffRequest {
    QString worktree;
    QString oid; // the commit, or HEAD for uncommitted work
    QString path;
    QString oldPath;
    DiffSide side = DiffSide::Commit;
    bool untracked = false;
};

// Where one side of a file lives: a git object (`rev:path`, `:path` for the
// index) or a file in the worktree.
struct BlobSource {
    enum class Kind { None, Object, File };
    Kind kind = Kind::None;
    QString spec;
    friend bool operator==(const BlobSource &, const BlobSource &) = default;
};

struct LfsPointer {
    QString oid;
    qint64 size = -1;
};

// Files a line diff says nothing useful about get a card instead.
struct SpecialFile {
    enum class Kind { None, Binary, Image, Submodule, Lfs, Empty };
    Kind kind = Kind::None;
    BlobSource oldBlob;
    BlobSource newBlob;
    qint64 oldSize = -1;
    qint64 newSize = -1;
    QString fromCommit;
    QString toCommit;
    bool dirty = false;      // the submodule's worktree has changes of its own
    bool checkedOut = false; // the submodule is there to read its log from
    QStringList commits;     // "abc1234 subject", newest first
    std::optional<LfsPointer> lfsOld;
    std::optional<LfsPointer> lfsNew;
};

struct FileDiff {
    QString worktree;
    DiffDocument document;
    SpecialFile special;
};

namespace detail {

bool isImagePath(const QString &path);
std::optional<LfsPointer> parseLfsPointer(const QStringList &lines);
// Classifies from the patch alone; sizes, blobs and logs are filled by git.
SpecialFile classifySpecial(const DiffDocument &document);
BlobSource oldBlobFor(const DiffRequest &request, const DiffFileMeta &meta);
BlobSource newBlobFor(const DiffRequest &request, const DiffFileMeta &meta);

} // namespace detail

} // namespace GitNaga
