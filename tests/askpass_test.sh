#!/usr/bin/env bash
# git asks GitNaga for credentials: `git credential fill` runs the same
# askpass path a fetch over https does, with no server needed. The dialog is
# answered through real key events (GITNAGA_ASKPASS_TYPE), offscreen.
set -euo pipefail
gitnaga=${1:?usage: askpass_test.sh <gitnaga binary>}
export QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_QPA_PLATFORMTHEME=
export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_NOSYSTEM=1 GIT_TERMINAL_PROMPT=0
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
cd "$scratch"

answer=$(GITNAGA_ASKPASS_TYPE=hunter2 "$gitnaga" --askpass "Password for 'https://ada@example.invalid': ")
[[ $answer == hunter2 ]] || { echo "explicit --askpass printed '$answer'"; exit 1; }

filled=$(printf 'protocol=https\nhost=example.invalid\n\n' |
    GIT_ASKPASS="$gitnaga" GITNAGA_ASKPASS=1 GITNAGA_ASKPASS_TYPE=s3cret git credential fill)
grep -qx 'username=s3cret' <<<"$filled" || { echo "no username from askpass: $filled"; exit 1; }
grep -qx 'password=s3cret' <<<"$filled" || { echo "no password from askpass"; exit 1; }

# Escape cancels: exit 1 and nothing on stdout, so git reports the failure.
if GITNAGA_ASKPASS_TYPE=typed GITNAGA_ASKPASS_CANCEL=1 "$gitnaga" --askpass "Passphrase for key:" >out.txt; then
    echo "cancel exited 0"; exit 1
fi
[[ ! -s out.txt ]] || { echo "cancel printed an answer"; exit 1; }
echo "askpass ok"
