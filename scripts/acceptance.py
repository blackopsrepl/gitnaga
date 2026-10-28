#!/usr/bin/env python3
"""Visual acceptance: drive the real GitNaga window through every fixture.

    scripts/acceptance.py [--build build/dev] [--out DIR] [--zoo DIR] [--big N] [only...]

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
    parser.add_argument("only", nargs="*")
    args = parser.parse_args()

    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    zoo = Path(args.zoo).resolve() if args.zoo else out / "zoo"
    subprocess.run([str(ROOT / "scripts/fixture-zoo.sh"), str(zoo), "--big", str(args.big)],
                   check=True, stdout=subprocess.DEVNULL)

    binary = Path(args.build) / "gitnaga"
    env = {**os.environ, "QT_QPA_PLATFORM": "offscreen", "QT_QUICK_BACKEND": "software",
           "QT_QPA_PLATFORMTHEME": "", "XDG_CONFIG_HOME": str(out / "config")}
    selected = scenarios(zoo, out)
    names = args.only or list(selected)
    results = [run(binary, name, selected[name], env) for name in names]
    print(f"\n{sum(results)}/{len(results)} scenarios passed; screenshots in {out}")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
