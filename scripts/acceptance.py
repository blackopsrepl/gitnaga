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
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def scenarios(zoo: Path, out: Path):
    """name -> steps. `expect` steps are checked here, not by the app."""
    def shot(name):
        return {"shot": str(out / f"{name}.png")}

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
        "submodule": [
            {"open": str(zoo / "submodule")}, {"idle": True}, {"select": "WORKTREE"}, {"idle": True},
            shot("submodule"),
        ],
        "lfs": [
            {"open": str(zoo / "lfs")}, {"idle": True}, {"select": 0}, {"idle": True}, shot("lfs"),
        ],
        "big": [
            {"open": str(zoo / "big")}, {"idle": True}, shot("big"),
            {"print": "commitCount"},
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
