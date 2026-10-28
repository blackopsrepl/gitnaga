#!/usr/bin/env bash
# Builds the fixture zoo: one small repository per state GitNaga must handle,
# so every release is looked at against the same cases.
#
#   scripts/fixture-zoo.sh <dir> [--big N]
#
# Each repository is created from scratch; the directory is wiped first.
set -euo pipefail

root=${1:?usage: fixture-zoo.sh <dir> [--big N]}
big=0
if [[ ${2:-} == --big ]]; then big=${3:?--big needs a commit count}; fi

rm -rf "$root"
mkdir -p "$root"
cd "$root"

export GIT_AUTHOR_NAME="Ada Lovelace" GIT_AUTHOR_EMAIL="ada@zoo.invalid"
export GIT_COMMITTER_NAME="Ada Lovelace" GIT_COMMITTER_EMAIL="ada@zoo.invalid"
export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_NOSYSTEM=1

commit() { # repo file content message
    printf '%s\n' "$3" >>"$1/$2"
    git -C "$1" add "$2"
    git -C "$1" commit -qm "$4"
}

new_repo() {
    git init -q -b main "$1"
    git -C "$1" config user.name "Ada Lovelace"
    git -C "$1" config user.email "ada@zoo.invalid"
    git -C "$1" config commit.gpgsign false
}

# A shared upstream every clone below can push to and fall behind.
new_repo seed
for i in 1 2 3; do commit seed readme.md "line $i" "docs: line $i"; done
git clone -q --bare seed remote.git

# branches: local work ahead, upstream ahead, merged and unmerged lines.
git clone -q remote.git branches
git -C branches config user.name "Ada Lovelace"; git -C branches config user.email "ada@zoo.invalid"
git -C branches switch -qc feature/search
commit branches search.txt "search" "feat(search): add search"
commit branches search.txt "more" "feat(search): refine search"
git -C branches push -q -u origin feature/search
git -C branches switch -q main
commit branches main.txt "local" "fix: local only work"
git -C branches switch -qc topic/merged main
commit branches merged.txt "merged" "feat: merged topic"
git -C branches switch -q main
git -C branches merge -q --no-ff topic/merged -m "Merge branch 'topic/merged'"
(cd seed && git pull -q ../remote.git main && commit . upstream.txt "upstream" "feat: upstream change" && git push -q ../remote.git main)
git -C branches fetch -q

# tags: lightweight and annotated, both must show.
new_repo tags
for i in 1 2 3 4; do commit tags f.txt "$i" "chore: release $i"; git -C tags tag -a "v0.$i.0" -m "release $i"; done
git -C tags tag lightweight HEAD~1

# internal: tool namespaces, a filter-branch backup, a stash, a remote HEAD.
git clone -q remote.git internal
git -C internal config user.name "Ada Lovelace"; git -C internal config user.email "ada@zoo.invalid"
commit internal a.txt "a" "feat: a"
git -C internal update-ref refs/codex/snapshots/abc HEAD~1
git -C internal update-ref refs/original/refs/heads/main HEAD~2
git -C internal update-ref refs/prefetch/remotes/origin/main HEAD
echo stash >>internal/a.txt
git -C internal stash push -qm "work in flight"

# dirty: staged, unstaged, both, untracked, rename, deletion.
new_repo dirty
commit dirty keep.txt "keep" "init"
commit dirty both.txt "one" "both"
commit dirty gone.txt "gone" "gone"
commit dirty old-name.txt "$(printf 'alpha\nbeta\ngamma\ndelta')" "rename source"
echo staged >>dirty/keep.txt && git -C dirty add keep.txt
echo two >>dirty/both.txt && git -C dirty add both.txt && echo three >>dirty/both.txt
git -C dirty rm -q gone.txt
git -C dirty mv old-name.txt new-name.txt
echo untracked >dirty/untracked.txt

# detached: HEAD on an older commit.
new_repo detached
for i in 1 2 3; do commit detached f.txt "$i" "commit $i"; done
git -C detached checkout -q HEAD~1

# conflict: a merge stopped on a conflict.
new_repo conflict
commit conflict f.txt "base" "base"
git -C conflict switch -qc other
printf 'theirs\n' >conflict/f.txt && git -C conflict commit -qam "theirs"
git -C conflict switch -q main
printf 'ours\n' >conflict/f.txt && git -C conflict commit -qam "ours"
git -C conflict merge other >/dev/null 2>&1 || true

# rebase: a rebase stopped on a conflict.
new_repo rebase
commit rebase f.txt "base" "base"
git -C rebase switch -qc topic
printf 'topic\n' >rebase/f.txt && git -C rebase commit -qam "topic change"
git -C rebase switch -q main
printf 'main\n' >rebase/f.txt && git -C rebase commit -qam "main change"
git -C rebase switch -q topic
git -C rebase rebase main >/dev/null 2>&1 || true

# conflict-blocks: a merge stopped with two conflict blocks (diff3 style, so
# the common ancestor shows) in one file, and a second conflicted file.
new_repo conflict-blocks
git -C conflict-blocks config merge.conflictStyle diff3
printf 'title\nalpha\nshared 1\nshared 2\nshared 3\nshared 4\nshared 5\nomega\n' >conflict-blocks/notes.txt
printf 'port = 80\n' >conflict-blocks/config.ini
git -C conflict-blocks add . && git -C conflict-blocks commit -qm "base"
git -C conflict-blocks switch -qc feature/notes
sed -i 's/alpha/alpha from feature/; s/omega/omega from feature/' conflict-blocks/notes.txt
printf 'port = 8080\n' >conflict-blocks/config.ini
git -C conflict-blocks commit -qam "feat: feature edits"
git -C conflict-blocks switch -q main
sed -i 's/alpha/alpha from main/; s/omega/omega from main/' conflict-blocks/notes.txt
printf 'port = 443\n' >conflict-blocks/config.ini
git -C conflict-blocks commit -qam "fix: main edits"
git -C conflict-blocks merge feature/notes >/dev/null 2>&1 || true

# bisect: a bisect session under way, one good and one bad commit marked.
new_repo bisect
for i in 1 2 3 4 5 6; do commit bisect f.txt "$i" "change $i"; done
git -C bisect bisect start HEAD HEAD~5 >/dev/null

# submodule: a repository with one submodule, its pointer moved.
new_repo submodule
commit submodule root.txt "root" "init"
git -C submodule -c protocol.file.allow=always submodule add -q "$root/seed" lib >/dev/null 2>&1
git -C submodule commit -qm "add submodule"
commit submodule/lib extra.txt "extra" "move submodule"

# lfs: a pointer file, the way a repository without git-lfs installed sees it.
new_repo lfs
printf 'version https://git-lfs.github.com/spec/v1\noid sha256:%064d\nsize 1048576\n' 0 >lfs/asset.bin
git -C lfs add asset.bin && git -C lfs commit -qm "add asset"

# big: N commits across several long-lived branches, through fast-import.
if (( big > 0 )); then
    new_repo big
    python3 - "$big" <<'PY' | git -C big fast-import --quiet
import sys
n = int(sys.argv[1])
branches = ["main", "dev", "release", "feature/a", "feature/b"]
marks = {}
mark = 0
for i in range(n):
    mark += 1
    b = branches[i % len(branches)] if i > 10 else "main"
    print(f"commit refs/heads/{b}")
    print(f"mark :{mark}")
    print(f"committer Ada Lovelace <ada@zoo.invalid> {1600000000 + i * 60} +0000")
    msg = f"change {i}"
    print(f"data {len(msg)}\n{msg}")
    if b in marks:
        print(f"from :{marks[b]}")
    elif "main" in marks:
        print(f"from :{marks['main']}")
    if i % 997 == 0 and b != "main" and "main" in marks and marks["main"] != marks.get(b):
        print(f"merge :{marks['main']}")
    print(f"M 644 inline f{i % 50}.txt")
    body = f"{i}\n"
    print(f"data {len(body)}\n{body}")
    marks[b] = mark
PY
    git -C big checkout -q main
fi

echo "fixture zoo ready in $root"
