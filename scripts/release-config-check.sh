#!/usr/bin/env bash
# Validate the release configuration and preview the next release.
# Read-only: never writes a version, changelog, or tag.
set -euo pipefail

root=$(git rev-parse --show-toplevel)
cd "$root"

fail() {
  printf 'release-check: %s\n' "$*" >&2
  exit 1
}

[ -f csemver.toml ] || fail "csemver.toml is missing"
command -v csemver >/dev/null || fail "csemver is not installed"

[ -f cmake/GitNagaVersion.cmake ] || fail "surface file is missing: cmake/GitNagaVersion.cmake"
grep -q 'cmake/GitNagaVersion.cmake' csemver.toml || fail "cmake/GitNagaVersion.cmake is not declared in csemver.toml"
[ -f packaging/io.gitnaga.GitNaga.metainfo.xml ] || fail "surface file is missing: the AppStream metainfo"
grep -q 'scripts/appstream-release.sh' csemver.toml || fail "the AppStream postbump script is not wired in csemver.toml"

version=$(sed -n 's/^set(GITNAGA_VERSION "\([^"]*\)")/\1/p' cmake/GitNagaVersion.cmake)
[ -n "$version" ] || fail "cannot read GITNAGA_VERSION from cmake/GitNagaVersion.cmake"

appstream=$(sed -n 's/.*<release version="\([^"]*\)".*/\1/p' \
  packaging/io.gitnaga.GitNaga.metainfo.xml | head -1)
[ -n "$appstream" ] || fail "no <release> entry in the AppStream metainfo"
[ "$appstream" = "$version" ] || fail "AppStream latest release $appstream != version $version"

[ -z "$(git status --porcelain)" ] || fail "working tree is dirty; commit or stash first"

if git rev-parse -q --verify "refs/tags/v$version" >/dev/null; then
  printf 'tag v%s exists\n' "$version"
else
  printf 'warning: tag v%s does not exist yet; treating this as the first release of the line\n' "$version"
fi

printf 'version %s, surfaces coherent, tree clean\n\n' "$version"
printf 'next release preview:\n'
csemver --dry-run "$@" 2>&1 | sed 's/^/  /'
