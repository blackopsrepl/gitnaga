# GitNaga: from demo to daily driver

Goal: a native git client that beats GitKraken for one person on Linux. No teams,
no accounts, no cloud. Audit date: 2026-10-28, against 0.1.22 (4df11f1).

## Diagnosis

The app grew feature by feature (worktrees, remote delete) on top of three
weak foundations. Most of the visible bugs come from these three:

1. **Refs are display strings.** `for-each-ref` reads every namespace with no
   filter and no peeling. Each ref becomes a string with a glyph prefix
   (`⇄ `, `# `), and QML parses those prefixes back out.
2. **Every change reloads everything.** Each refresh spawns about 13 git
   processes, rebuilds the full history, writes a new commit object to the
   repository and clears the selection.
3. **Tests stop below the controller.** The 9 test binaries exercise
   `GitClient`. The controller → QML path where the bugs live is untested.

The core loop is also missing: there is no fetch, pull, push, stash or conflict
handling anywhere in `src/` or `qml/`.

## Verified defects (observed on solverforge, 2563 commits, 93 refs)

| # | Defect | Evidence |
|---|--------|----------|
| 1 | Internal refs appear under Branches: `refs/codex/snapshots/…`, `refs/original/release/0.7.0`, `refs/stash` | screenshot |
| 2 | `origin/HEAD`-style symbolic refs appear as remote branches | screenshot |
| 3 | 81 of 84 tags are invisible: annotated tags are not peeled (`%(*objectname)`) | `for-each-ref` objecttype count |
| 4 | Selecting the uncommitted row sets `selectedOid = "HEAD"`. `viewingWorktree` is then false, so the commit form never shows | controller probe |
| 5 | The uncommitted row's diff runs `git show HEAD -- path`, which is HEAD's change to the file, not the working change | code, `git_inspect.cpp`, `repository_controller.cpp:147` |
| 6 | Selecting the uncommitted row runs `git status`. That rewrites `.git/index`, the watcher fires a full refresh, and the refresh clears the selection about 1 s later | controller probe |
| 7 | Every refresh writes new objects into the user's repository (`git add --all` + `commit-tree`), including blobs for large untracked files | 756 → 758 objects in one probe |
| 8 | Edits to worktree files are not watched, so the uncommitted row goes stale until something under `.git` changes | code |
| 9 | Every refresh clears the selection: any external `git` activity throws away what the user is reading | code |
| 10 | Every git call has a 30 s timeout, so a slow pre-commit hook or a slow network push is killed mid-run | `git_client.cpp` |
| 11 | Operations started while busy are dropped with no feedback | `runAsyncOperation` guard |
| 12 | The sidebar builds refs from the loaded commits, so refs beyond the 5000-commit cap disappear | `rebuildReferences` |
| 13 | Merge uses the oid, not the branch, which gives "Merge commit 'abc…'" messages. With a detached HEAD the menu labels read "onto " with an empty name | code |
| 14 | Graph column is mostly empty while commit subjects truncate at about 40 characters. The meta line reads "Name  , 1h ago" | screenshot |

## Plan

### 0.2 — Foundation (a feature freeze until this lands)

- **Typed `Ref` model in C++**: kind, short name, remote, peeled target,
  symbolic flag, upstream, ahead/behind. QML never parses ref strings.
- **Ref visibility policy**: show heads, remotes and tags by default. Stash
  gets its own section. Remote `HEAD`, `refs/prefetch`, `refs/original`,
  notes and tool namespaces (`refs/codex`, `refs/jj`, `refs/gitbutler`, …)
  go behind a "Show internal refs" toggle. The graph walks only visible refs,
  not `--all`.
- **Work in progress without writing objects**: build it from
  `git status --porcelain=v2 -z` plus `git diff` / `git diff --cached` per
  file. Delete the snapshot-commit machinery.
- **Split refresh**: a cheap status refresh (worktree watcher, debounced,
  respects ignores) and a history refresh that runs only when the ref
  fingerprint changes. Keep the selection and scroll position by oid across
  both.
- **Process runner**: no blanket timeout, cancellable, streamed progress,
  operations queued instead of dropped.
- **Lazy history**: stream `git log` in pages and lay lanes out
  incrementally. Remove the cap; the sidebar comes from `for-each-ref`, never
  from loaded commits.

### 0.3 — Core loop parity

- Fetch, pull and push with ahead/behind on every branch, set-upstream,
  force-with-lease (confirmed).
- Staging per file, hunk and line. Amend. Discard (confirmed).
- Stash: create, list, apply, pop, drop, shown in the graph.
- In-progress operation banner (merge, rebase, cherry-pick, revert) with
  continue, abort and skip, a conflicted-file list, ours/theirs per file,
  and an external `mergetool` hand-off.
- Undo the last operation (reflog-backed).
- Search: message, author, sha, path.

### 0.4 — Where GitKraken loses

- Drag-to-rebase in the graph: reorder, squash, fixup, reword.
- Hide or solo a branch in the graph.
- Instant open and smooth scroll at 1M commits (commit-graph file, paged
  layout).
- Keyboard command palette over every action.
- File history and blame. Side-by-side, word-level, syntax-highlighted diff.
- Submodule and LFS awareness.

## Process

- **Controller-level tests** against a fixture repo zoo: annotated tags,
  stash, tool refs, detached HEAD, mid-rebase, conflicts, bare repo,
  submodule, 100k commits. A 30-line probe found defects 4, 6 and 7, which
  the existing suite does not catch.
- **Visual acceptance per release**: headless sway + grim screenshots of the
  zoo, inspected before the tag.
- **Dogfood**: GitNaga is the daily client on solverforge. Usage friction is
  filed as an issue the same day.
