#!/usr/bin/env bash
# Prove that a release is published: the branch head and the annotated tag
# resolve to the same objects locally and on every remote, with zero
# divergence. Read-only against the remotes.
set -euo pipefail

root=$(git rev-parse --show-toplevel)
cd "$root"

branch=$(git rev-parse --abbrev-ref HEAD)
version=$(sed -n 's/^set(GITNAGA_VERSION "\([^"]*\)")/\1/p' cmake/GitNagaVersion.cmake)
[ -n "$version" ] || { printf 'cannot read GITNAGA_VERSION\n' >&2; exit 1; }
tag="v$version"
remotes=${REMOTES:-github origin}

local_head=$(git rev-parse HEAD)
git rev-parse -q --verify "refs/tags/$tag" >/dev/null \
  || { printf 'local tag %s does not exist\n' "$tag" >&2; exit 1; }
local_tag=$(git rev-parse "$tag")
local_peel=$(git rev-parse "$tag^{}")

printf 'release %s branch %s\n' "$tag" "$branch"
printf 'local head   %s\n' "$local_head"
printf 'local tag    %s -> %s\n\n' "$local_tag" "$local_peel"

status=0
for remote in $remotes; do
  printf '== %s ==\n' "$remote"
  if ! git remote get-url "$remote" >/dev/null 2>&1; then
    printf '  remote not configured\n'
    status=1
    continue
  fi

  if ! output=$(git ls-remote "$remote" "refs/heads/$branch" "refs/tags/$tag" "refs/tags/$tag^{}"); then
    printf '  ls-remote failed\n'
    status=1
    continue
  fi

  remote_head=$(printf '%s\n' "$output" | awk -v r="refs/heads/$branch" '$2 == r { print $1 }')
  remote_tag=$(printf '%s\n' "$output" | awk -v r="refs/tags/$tag" '$2 == r { print $1 }')
  remote_peel=$(printf '%s\n' "$output" | awk -v r="refs/tags/$tag^{}" '$2 == r { print $1 }')

  printf '  head   %s\n' "$remote_head"
  printf '  tag    %s\n' "$remote_tag"
  printf '  peeled %s\n' "$remote_peel"

  [ "$remote_head" = "$local_head" ] || { printf '  FAIL branch head mismatch\n'; status=1; }
  [ "$remote_tag" = "$local_tag" ] || { printf '  FAIL tag object mismatch\n'; status=1; }
  [ "$remote_peel" = "$local_peel" ] || { printf '  FAIL tag target mismatch\n'; status=1; }

  if git fetch --quiet "$remote" "$branch" 2>/dev/null; then
    counts=$(git rev-list --left-right --count "$branch...FETCH_HEAD")
    printf '  divergence %s\n' "$counts"
    [ "$counts" = "$(printf '0\t0')" ] || { printf '  FAIL divergence is not 0 0\n'; status=1; }
  fi
done

if [ "$status" -eq 0 ]; then
  printf '\nrelease %s verified on: %s\n' "$tag" "$remotes"
else
  printf '\nrelease verification FAILED\n' >&2
  exit 1
fi
