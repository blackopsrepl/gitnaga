#!/usr/bin/env bash
# csemver prebump: open a slot for the coming release in the AppStream
# history. AppStream keeps one <release> element per release, newest first,
# and a csemver regex updater can only rewrite a version in place, so this
# prepends a copy of the newest entry dated today; csemver's bump then moves
# that copy's version to the new one and commits the file with the release.
set -euo pipefail

root=$(git rev-parse --show-toplevel)
cd "$root"
metainfo=packaging/io.gitnaga.GitNaga.metainfo.xml

current=$(sed -n 's/^set(GITNAGA_VERSION "\([^"]*\)")/\1/p' cmake/GitNagaVersion.cmake)
[ -n "$current" ] || { echo "appstream-release: no GITNAGA_VERSION" >&2; exit 1; }
grep -q "<release version=\"$current\"" "$metainfo" \
    || { echo "appstream-release: newest AppStream release is not $current" >&2; exit 1; }
entry="<release version=\"$current\" date=\"$(date -u +%F)\"/>"
sed -i "0,/<releases>/s|<releases>|<releases>\n    $entry|" "$metainfo"
