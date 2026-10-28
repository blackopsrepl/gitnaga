#!/usr/bin/env bash
# The window's own Fetch against real authenticating servers: an HTTP remote
# behind basic auth (git asks for a username and password) and an ssh remote
# whose key has a passphrase (ssh asks through SSH_ASKPASS). GitNaga answers
# both through its dialog, typed into offscreen; a wrong answer must fail.
set -euo pipefail
gitnaga=$(realpath "${1:?usage: transport_askpass_test.sh <gitnaga binary>}")
here=$(cd "$(dirname "$0")" && pwd)
export QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_QPA_PLATFORMTHEME=
export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_NOSYSTEM=1
export GIT_AUTHOR_NAME=Ada GIT_AUTHOR_EMAIL=ada@zoo.invalid GIT_COMMITTER_NAME=Ada GIT_COMMITTER_EMAIL=ada@zoo.invalid
unset SSH_AUTH_SOCK SSH_ASKPASS GIT_ASKPASS DISPLAY
scratch=$(mktemp -d)
pids=()
cleanup() {
    for pid in "${pids[@]}"; do kill "$pid" 2>/dev/null || true; done
    [[ -n ${KEEP:-} ]] || rm -rf "$scratch"; echo "scratch $scratch" >&2
}
trap cleanup EXIT
cd "$scratch"
export XDG_CONFIG_HOME=$scratch/config

free_port() { python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1])'; }

# One bare remote, a clone to fetch into, and a commit the clone lacks.
git init -q --bare -b main remote.git
git clone -q remote.git seed 2>/dev/null
(cd seed && echo one >f && git add f && git commit -qm one && git push -q origin main)
git clone -q remote.git mine
(cd seed && echo two >>f && git commit -qam two && git push -q origin main)

fetch_with() { # remote-url typed-answer -> prints lastOperationOk and behind
    git -C mine remote set-url origin "$1"
    cat >drive.json <<EOF
[{"open": "$scratch/mine"}, {"idle": true}, {"call": "sync.fetch"}, {"wait": 300}, {"idle": true},
 {"print": "lastOperationOk"}, {"print": "behind"}, {"print": "operationMessage"}, {"quit": true}]
EOF
    GITNAGA_ASKPASS_TYPE=$2 GITNAGA_DRIVE=$scratch/drive.json timeout 120 "$gitnaga" 2>>app.log
    git -C mine update-ref -d refs/remotes/origin/main 2>/dev/null || true
    git -C mine update-ref refs/remotes/origin/main "$(git -C mine rev-parse main)"
}

expect() { # label output wanted-ok [wanted-message]
    local ok
    ok=$(sed -n 's/^lastOperationOk=//p' <<<"$2")
    if [[ $ok != "$3" ]]; then echo "$1: lastOperationOk=$ok, wanted $3"; echo "$2"; exit 1; fi
    if [[ $3 == true ]] && ! grep -qx 'behind=1' <<<"$2"; then echo "$1: fetched nothing"; echo "$2"; exit 1; fi
    if [[ -n ${4:-} ]] && ! grep -q "^operationMessage=$4" <<<"$2"; then echo "$1: message is not '$4'"; echo "$2"; exit 1; fi
    echo "ok: $1 — $(sed -n 's/^operationMessage=//p' <<<"$2")"
}

# HTTP with basic auth: the dialog answers both prompts with the same text.
http_port=$(free_port)
python3 "$here/git_http_auth_server.py" "$scratch" "$http_port" s3cret:s3cret 2>http.log &
pids+=($!)
for _ in $(seq 50); do curl -s -o /dev/null "http://127.0.0.1:$http_port/" && break; sleep 0.1; done
url="http://127.0.0.1:$http_port/remote.git"
expect "http, right password" "$(fetch_with "$url" s3cret)" true
expect "http, wrong password" "$(fetch_with "$url" nope)" false "The remote refused the username or password"

# ssh with a passphrase-protected key and an unprivileged sshd.
ssh-keygen -q -t ed25519 -N '' -f host_key
ssh-keygen -q -t ed25519 -N s3cret -f user_key
cp user_key.pub authorized_keys
ssh_port=$(free_port)
cat >sshd_config <<EOF
Port $ssh_port
ListenAddress 127.0.0.1
HostKey $scratch/host_key
AuthorizedKeysFile $scratch/authorized_keys
PubkeyAuthentication yes
PasswordAuthentication no
KbdInteractiveAuthentication no
StrictModes no
UsePAM no
PidFile $scratch/sshd.pid
EOF
/usr/sbin/sshd -D -f "$scratch/sshd_config" -E "$scratch/sshd.log" &
pids+=($!)
for _ in $(seq 50); do (exec 3<>/dev/tcp/127.0.0.1/$ssh_port) 2>/dev/null && break; sleep 0.1; done
export GIT_SSH_COMMAND="ssh -F /dev/null -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o IdentitiesOnly=yes -o IdentityFile=$scratch/user_key -o BatchMode=no"
url="ssh://$(id -un)@127.0.0.1:$ssh_port$scratch/remote.git"
expect "ssh, right passphrase" "$(fetch_with "$url" s3cret)" true
expect "ssh, wrong passphrase" "$(fetch_with "$url" nope)" false "The remote refused the ssh key"
echo "transport askpass ok"
