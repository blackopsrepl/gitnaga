#!/usr/bin/env bash
# Push the current branch and the current release tag to every configured
# remote, then prove the result.
#
# GitHub uses the configured credential helper. The local Forgejo push uses a
# one-use write token generated through the operate-local-forgejo helper when
# it is available, otherwise it falls back to the ambient git credentials.
set -euo pipefail

root=$(git rev-parse --show-toplevel)
cd "$root"

github_remote=${GITHUB_REMOTE:-github}
forgejo_remote=${FORGEJO_REMOTE:-origin}
helper_dir=${FORGEJO_HELPER_DIR:-/home/pvd/.config/opencode/skill/operate-local-forgejo/scripts}

branch=$(git rev-parse --abbrev-ref HEAD)
version=$(sed -n 's/^set(GITNAGA_VERSION "\([^"]*\)")/\1/p' cmake/GitNagaVersion.cmake)
[ -n "$version" ] || { printf 'cannot read GITNAGA_VERSION\n' >&2; exit 1; }
tag="v$version"

git rev-parse -q --verify "refs/tags/$tag" >/dev/null \
  || { printf 'local tag %s does not exist; run make release first\n' "$tag" >&2; exit 1; }

printf 'pushing %s and %s\n' "$branch" "$tag"

if git remote get-url "$github_remote" >/dev/null 2>&1; then
  printf '  -> %s\n' "$github_remote"
  git push "$github_remote" "$branch"
  git push "$github_remote" "$tag"
else
  printf '  -> %s skipped (remote not configured)\n' "$github_remote"
fi

if [ -f "$helper_dir/forgejo-common.sh" ]; then
  printf '  -> %s (one-use token)\n' "$forgejo_remote"
  # shellcheck source=/dev/null
  source "$helper_dir/forgejo-common.sh"
  forgejo_preflight
  name="opencode-forgejo-push-${tag}-$(date +%Y%m%d%H%M%S)-$$"
  token=''
  cleanup() { forgejo_remove_token "$name"; }
  trap cleanup EXIT
  token=$(forgejo_issue_token "$name" write:repository)
  basic=$(forgejo_basic_header "$token")
  git -c credential.helper= -c http.extraHeader="Authorization: Basic $basic" push "$forgejo_remote" "$branch"
  git -c credential.helper= -c http.extraHeader="Authorization: Basic $basic" push "$forgejo_remote" "$tag"
  forgejo_remove_token "$name"
  trap - EXIT
else
  printf '  -> %s (ambient credentials)\n' "$forgejo_remote"
  git push "$forgejo_remote" "$branch"
  git push "$forgejo_remote" "$tag"
fi
