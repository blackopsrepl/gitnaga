#!/usr/bin/env python3
"""Visual acceptance: drive the real GitNaga window through every fixture.

    scripts/acceptance.py [--build build/dev] [--out DIR] [--zoo DIR] [--big N] [--huge] [only...]

Builds the fixture zoo (scripts/fixture-zoo.sh), then runs one scripted
session per scenario through GITNAGA_DRIVE (see src/ui_driver.hpp), offscreen
with the software renderer, so no compositor is needed. Every scenario saves
its screenshots to OUT; a scenario fails when the app exits non-zero or an
`expect` does not hold. Look at the screenshots: a green run proves the
script ran, not that the screen is right.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def scenarios(zoo: Path, out: Path):
    """name -> steps. `expect` steps are checked here, not by the app."""
    def shot(name):
        return {"shot": str(out / f"{name}.png")}

    def sandbox(name):
        """A private copy of `branches` and its remote, for scenarios that
        push and pull: they must not move the zoo under the others."""
        root = out / "sandbox" / name
        shutil.rmtree(root, ignore_errors=True)
        root.mkdir(parents=True)
        shutil.copytree(zoo / "remote.git", root / "remote.git")
        shutil.copytree(zoo / "branches", root / "branches", symlinks=True)
        subprocess.run(["git", "-C", str(root / "branches"), "remote", "set-url", "origin",
                        str(root / "remote.git")], check=True)
        return str(root / "branches")

    return {
        "branches": [
            {"open": str(zoo / "branches")}, {"idle": True}, shot("branches"),
            {"print": "currentBranch"}, {"print": "ahead"}, {"print": "behind"},
            {"expect": {"currentBranch": "main", "ahead": "3", "behind": "1"}},
        ],
        "tags": [
            {"open": str(zoo / "tags")}, {"idle": True}, shot("tags"),
        ],
        "internal-hidden": [
            {"open": str(zoo / "internal")}, {"idle": True}, shot("internal-hidden"),
        ],
        "internal-shown": [
            {"open": str(zoo / "internal")}, {"idle": True},
            {"set": "showInternalRefs", "value": True}, {"idle": True}, shot("internal-shown"),
            {"set": "showInternalRefs", "value": False}, {"idle": True},
        ],
        "dirty": [
            {"open": str(zoo / "dirty")}, {"idle": True},
            {"select": "WORKTREE"}, {"idle": True}, shot("dirty-worktree"),
            {"print": "uncommittedCount"}, {"expect": {"uncommittedCount": "5"}},
            {"file": 4}, {"idle": True}, shot("dirty-untracked"),
        ],
        "detached": [
            {"open": str(zoo / "detached")}, {"idle": True}, shot("detached"),
            {"print": "detached"}, {"expect": {"detached": "true"}},
        ],
        "conflict": [
            {"open": str(zoo / "conflict")}, {"idle": True}, {"select": "WORKTREE"}, {"idle": True},
            shot("conflict"), {"print": "operationInProgress"}, {"expect": {"operationInProgress": "merge"}},
        ],
        "rebase": [
            {"open": str(zoo / "rebase")}, {"idle": True}, shot("rebase"),
            {"print": "operationInProgress"}, {"expect": {"operationInProgress": "rebase"}},
        ],
        "conflict-view": [
            {"open": str(zoo / "conflict-blocks")}, {"idle": True}, shot("conflict-banner"),
            {"click": [639, 181]}, {"wait": 600}, shot("conflict-view-open"),
            {"click": [1074, 149]}, {"click": [1153, 371]}, {"wait": 300}, shot("conflict-view-chosen"),
            {"click": [1278, 830]}, {"idle": True}, shot("conflict-view-saved"),
            {"print": "operationInProgress"}, {"expect": {"operationInProgress": "merge"}},
        ],
        "conflict-resolve": [
            {"open": str(zoo / "conflict")}, {"idle": True},
            {"call": "resolveConflict", "args": ["f.txt", "theirs"]}, {"idle": True}, shot("conflict-resolved"),
            {"call": "continueOperation"}, {"idle": True}, shot("conflict-continued"),
            {"print": "operationInProgress"}, {"expect": {"operationInProgress": ""}},
        ],
        "bisect": [
            {"open": str(zoo / "bisect")}, {"idle": True}, shot("bisect"),
            {"print": "operationInProgress"}, {"expect": {"operationInProgress": "bisect"}},
        ],
        "undo": [
            {"open": str(zoo / "branches")}, {"idle": True},
            {"call": "deleteBranch", "args": ["topic/merged"]}, {"idle": True}, shot("undo-available"),
            {"key": "Ctrl+Z"}, {"idle": True}, shot("undo-done"),
            {"print": "operationMessage"}, {"expect": {"operationMessage": "Undid delete branch topic/merged"}},
        ],
        "linear": [
            {"open": str(zoo / "linear")}, {"idle": True}, shot("linear"),
            {"rightClick": [500, 272]}, {"wait": 300}, shot("linear-menu"),
            {"click": [594, 472]}, {"wait": 600}, shot("rebase-editor-open"),
            {"drag": [319, 166, 319, 252]}, {"wait": 200},
            {"click": [800, 326]}, {"key": "s"}, {"click": [800, 246]}, {"key": "f"},
            {"click": [800, 206]}, {"key": "d"}, {"click": [800, 166]}, {"key": "r"}, {"wait": 200},
            {"click": [750, 245]}, {"key": "Ctrl+A"}, {"type": "feat(parser): tokenize and parse expressions"},
            {"wait": 200}, shot("rebase-editor"),
            {"click": [1124, 600]}, {"idle": True}, {"wait": 300}, shot("rebase-result"),
            {"print": "commitCount"}, {"print": "operationMessage"},
            {"expect": {"commitCount": "5", "operationMessage": "Rebased 5 commits"}},
        ],
        "branch-drag": [
            {"open": str(zoo / "drag")}, {"idle": True},
            {"drag": [870, 84, 640, 306], "release": False}, {"wait": 200}, shot("branch-drag"),
            {"release": [640, 306]}, {"wait": 300}, shot("branch-drop-menu"),
            {"key": "Escape"}, {"wait": 200},
            {"drag": [862, 120, 884, 300], "release": False}, {"wait": 200}, shot("branch-drag-chip"),
            {"release": [884, 300]}, {"wait": 300}, shot("branch-chip-drop-menu"),
            {"key": "Escape"}, {"wait": 200},
            {"drag": [870, 84, 640, 306]}, {"wait": 300}, {"click": [678, 384]}, {"wait": 300},
            shot("branch-drop-confirm"), {"click": [703, 487]}, {"idle": True}, shot("branch-drop-result"),
            {"print": "commitCount"}, {"expect": {"commitCount": "7"}},
        ],
        "edit-message": [
            {"open": str(zoo / "quick")}, {"idle": True},
            {"rightClick": [500, 236]}, {"wait": 300}, {"click": [560, 462]}, {"idle": True}, {"wait": 300},
            {"key": "Ctrl+A"}, {"type": "feat(parser): parse expressions by precedence"}, shot("edit-message"),
            {"click": [887, 580]}, {"idle": True},
            {"rightClick": [500, 164]}, {"wait": 300}, shot("quick-actions-menu"), {"click": [560, 416]}, {"idle": True},
            {"rightClick": [500, 236]}, {"wait": 300}, {"click": [560, 514]}, {"wait": 300}, shot("drop-confirm"),
            {"click": [703, 487]}, {"idle": True}, shot("quick-actions-result"),
            {"print": "commitCount"}, {"expect": {"commitCount": "6"}},
        ],
        "hide-solo": [
            {"open": str(zoo / "branches")}, {"idle": True}, {"call": "showAllRefs"}, {"idle": True},
            {"click": [282, 195]}, {"idle": True}, {"click": [282, 245]}, {"idle": True}, shot("hidden-refs"),
            {"print": "hiddenRefs"},
            {"expect": {"hiddenRefs": "refs/heads/topic/merged,refs/remotes/origin/feature/search"}},
        ],
        # A second process: hidden refs must have survived the restart.
        "hide-persist": [
            {"open": str(zoo / "branches")}, {"idle": True}, shot("hidden-after-restart"),
            {"print": "hiddenRefs"},
            {"expect": {"hiddenRefs": "refs/heads/topic/merged,refs/remotes/origin/feature/search"}},
        ],
        "hide-clear": [
            {"open": str(zoo / "branches")}, {"idle": True},
            {"click": [285, 859]}, {"idle": True},
            {"call": "setSoloRef", "args": ["refs/remotes/origin/main"]}, {"idle": True}, shot("solo-remote"),
            {"call": "setSoloRef", "args": ["refs/remotes/origin/main"]}, {"idle": True},
            {"print": "hiddenRefs"}, {"expect": {"hiddenRefs": ""}},
        ],
        "solo-tag": [
            {"open": str(zoo / "tags")}, {"idle": True},
            {"call": "setSoloRef", "args": ["refs/tags/v0.2.0"]}, {"idle": True}, shot("solo-tag"),
            {"print": "soloRef"}, {"print": "commitCount"},
            {"call": "setSoloRef", "args": ["refs/tags/v0.2.0"]}, {"idle": True},
            {"expect": {"soloRef": "refs/tags/v0.2.0"}},
        ],
        "submodule": [
            {"open": str(zoo / "submodule")}, {"idle": True}, {"select": "WORKTREE"}, {"idle": True},
            shot("submodule"),
        ],
        "lfs": [
            {"open": str(zoo / "lfs")}, {"idle": True}, {"select": 0}, {"idle": True}, shot("lfs"),
        ],
        **sync_scenarios(zoo, shot, sandbox),
        "big": [
            {"open": str(zoo / "big")}, {"idle": True}, shot("big"),
            {"print": "commitCount"},
        ],
        # The ancestry trace: a selection, then a hover on another line that
        # must add its own ancestry without dropping the selection's.
        "emphasis": [
            {"open": str(zoo / "branches")}, {"idle": True}, {"select": 2}, {"idle": True},
            shot("emphasis-select"), {"hover": [383, 180]}, {"hover": [383, 200]}, {"wait": 300}, shot("emphasis-hover"),
        ],
    }


def sync_scenarios(zoo: Path, shot, sandbox):
    """Fetch, pull and push against the zoo's own bare remote."""
    # A server that takes its time, so the footer can be seen mid-fetch.
    slow = sandbox("slow")
    subprocess.run(["git", "-C", slow, "config", "remote.origin.uploadpack", "sleep 3; git-upload-pack"], check=True)
    return {
        "sync-fetch-progress": [
            {"open": slow}, {"idle": True},
            {"call": "sync.fetch"}, {"wait": 800}, shot("sync-fetching"),
            {"click": [1382, 887]}, {"idle": True}, shot("sync-fetch-cancelled"),
            {"print": "operationMessage"}, {"expect": {"operationMessage": "Cancelled"}},
        ],
        "sync-toolbar": [
            {"open": str(zoo / "branches")}, {"idle": True}, shot("sync-toolbar"),
            {"click": [331, 20]}, {"wait": 300}, shot("sync-fetch-menu"), {"key": "Escape"},
            {"click": [411, 20]}, {"wait": 300}, shot("sync-pull-menu"), {"key": "Escape"},
        ],
        # main is 3 ahead and 1 behind: pull merges, then the push lands.
        "sync-pull-push": [
            {"open": sandbox("pull-push")}, {"idle": True},
            {"call": "sync.fetch"}, {"idle": True},
            {"call": "sync.pull"}, {"idle": True}, shot("sync-pulled"),
            {"print": "behind"}, {"expect": {"behind": "0"}},
            {"call": "sync.push"}, {"idle": True}, shot("sync-pushed"),
            {"print": "ahead"}, {"expect": {"ahead": "0"}},
        ],
        # Pushing the diverged main is refused; the lease replaces it.
        "sync-rejected": [
            {"open": sandbox("rejected")}, {"idle": True},
            {"call": "sync.push"}, {"idle": True}, shot("sync-rejected"),
            {"click": [785, 514]}, {"idle": True}, shot("sync-forced"),
            {"print": "ahead"}, {"print": "behind"}, {"expect": {"ahead": "0", "behind": "0"}},
        ],
        # topic/merged was never pushed: Pull is off, Push publishes it.
        "sync-publish": [
            {"open": sandbox("publish")}, {"idle": True},
            {"call": "checkoutBranch", "args": ["topic/merged"]}, {"idle": True}, shot("sync-no-upstream"),
            {"click": [410, 20]}, {"wait": 900}, shot("sync-pull-tooltip"),
            {"call": "sync.push"}, {"idle": True}, shot("sync-published"),
            {"print": "upstream"}, {"expect": {"upstream": "origin/topic/merged"}},
        ],
        "sync-upstream": [
            {"open": sandbox("upstream")}, {"idle": True},
            {"rightClick": [100, 195]}, {"wait": 300}, shot("sync-branch-menu"),
            {"click": [85, 81]}, {"wait": 400}, shot("sync-upstream-dialog"),
            {"click": [807, 502]}, {"idle": True}, shot("sync-upstream-set"),
            {"rightClick": [100, 195]}, {"wait": 300}, shot("sync-branch-menu-tracking"), {"key": "Escape"},
            {"call": "checkoutBranch", "args": ["topic/merged"]}, {"idle": True},
            {"print": "upstream"}, {"expect": {"upstream": "origin/feature/search"}},
        ],
        "sync-tag": [
            {"open": sandbox("tag")}, {"idle": True},
            {"call": "createTag", "args": ["v1.0", "HEAD"]}, {"idle": True},
            {"call": "sync.pushTag", "args": ["v1.0"]}, {"idle": True}, shot("sync-tag-pushed"),
        ],
    }


def huge_scenario(zoo: Path, out: Path):
    """--huge: a million commits, then select, hover and scroll deep into it."""
    def shot(name):
        return {"shot": str(out / f"{name}.png")}

    return [
        {"open": str(zoo / "big")}, {"idle": True}, shot("huge"), {"print": "commitCount"},
        {"expect": {"commitCount": "1000000"}},
        {"select": 0}, {"idle": True}, {"hover": [383, 180]}, {"hover": [383, 200]}, {"wait": 300}, shot("huge-hover"),
        *[{"wheel": [300, 400, -12000]} for _ in range(40)], {"wait": 300}, shot("huge-deep"),
    ]


def run(binary: Path, name: str, steps, env) -> bool:
    expectations = {}
    script = []
    for step in steps:
        if "expect" in step:
            expectations.update(step["expect"])
        else:
            script.append(step)
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as handle:
        json.dump(script, handle)
        path = handle.name
    try:
        result = subprocess.run([str(binary)], env={**env, "GITNAGA_DRIVE": path},
                                capture_output=True, text=True, timeout=300)
    finally:
        os.unlink(path)
    printed = dict(line.split("=", 1) for line in result.stdout.splitlines() if "=" in line)
    failures = [f"{key}: expected {value!r}, got {printed.get(key)!r}"
                for key, value in expectations.items() if printed.get(key) != value]
    ok = result.returncode == 0 and not failures
    mark = "✓" if ok else "✗"
    detail = ", ".join(f"{k}={v}" for k, v in printed.items())
    print(f"{mark} {name:18} {detail}")
    for failure in failures:
        print(f"    {failure}")
    if result.returncode != 0:
        print(f"    exit {result.returncode}: {result.stderr.strip()[-400:]}")
    return ok


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default=str(ROOT / "build/dev"))
    parser.add_argument("--out", default=str(ROOT / "build/acceptance"))
    parser.add_argument("--zoo", default=None)
    parser.add_argument("--big", type=int, default=100000)
    parser.add_argument("--huge", action="store_true",
                        help="build the big fixture at 1M commits and run the huge scenario")
    parser.add_argument("only", nargs="*")
    args = parser.parse_args()

    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    zoo = Path(args.zoo).resolve() if args.zoo else out / "zoo"
    big = 1000000 if args.huge else args.big
    marker = zoo / ".big-size"
    # A million-commit fixture takes minutes to write; reuse one of the right size.
    if not (args.zoo and marker.exists() and marker.read_text() == str(big)):
        subprocess.run([str(ROOT / "scripts/fixture-zoo.sh"), str(zoo), "--big", str(big)],
                       check=True, stdout=subprocess.DEVNULL)
        marker.write_text(str(big))

    binary = Path(args.build) / "gitnaga"
    env = {**os.environ, "QT_QPA_PLATFORM": "offscreen", "QT_QUICK_BACKEND": "software",
           "QT_QPA_PLATFORMTHEME": "", "XDG_CONFIG_HOME": str(out / "config")}
    selected = scenarios(zoo, out)
    if args.huge:
        selected["huge"] = huge_scenario(zoo, out)
    names = args.only or list(selected)
    results = [run(binary, name, selected[name], env) for name in names]
    print(f"\n{sum(results)}/{len(results)} scenarios passed; screenshots in {out}")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
