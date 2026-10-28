#!/usr/bin/env python3
"""Measure GitNaga on a huge history: load milestones, memory, interaction.

    scripts/perf_bench.py REPO [--build build/release] [--runs 3] [--no-interact]

Runs the real window offscreen through GITNAGA_DRIVE with GITNAGA_PERF=1:
opens REPO, waits for the history, then hovers, scrolls and selects through
the graph. Prints per run the milestones (ms since the open request), the
slowest GUI-thread blocks the app reported (anything over its frame budget),
the wall time and the peak RSS, then the median over the runs.
"""
import argparse
import json
import os
import re
import statistics
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MILESTONES = ("history: first rows in model", "graph: first rows painted", "history: loaded")


def script(repo: str, interact: bool):
    steps = [{"open": repo}, {"idle": True}, {"print": "commitCount"}]
    if interact:
        # Hover down the top rows (the full-ancestry worst case), scroll deep,
        # hover there, then select the top commit and hover again.
        steps += [{"hover": [300, 120 + 36 * i]} for i in range(12)]
        steps += [{"wheel": [300, 400, -1200]} for _ in range(20)]
        steps += [{"hover": [300, 140 + 36 * i]} for i in range(6)]
        steps += [{"select": 1}, {"wait": 200}]
        steps += [{"hover": [300, 120 + 36 * i]} for i in range(6)]
    return steps + [{"quit": True}]


def run_once(binary: Path, repo: str, interact: bool, env):
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as handle:
        json.dump(script(repo, interact), handle)
        path = handle.name
    try:
        result = subprocess.run(["/usr/bin/time", "-f", "TIME %e %M", str(binary)],
                                env={**env, "GITNAGA_DRIVE": path}, capture_output=True, text=True,
                                timeout=900)
    finally:
        os.unlink(path)
    marks, slow = {}, {}
    wall = rss = None
    for line in result.stderr.splitlines():
        if m := re.match(r"perf:\s+([\d.]+) ms\s+(.*)", line):
            marks.setdefault(m.group(2), float(m.group(1)))
        elif m := re.match(r"perf: slow (.*) ([\d.]+) ms", line):
            slow[m.group(1)] = max(slow.get(m.group(1), 0.0), float(m.group(2)))
        elif m := re.match(r"TIME ([\d.]+) (\d+)", line):
            wall, rss = float(m.group(1)), int(m.group(2)) // 1024
    count = next((l.split("=", 1)[1] for l in result.stdout.splitlines() if l.startswith("commitCount=")), "?")
    return {"marks": marks, "slow": slow, "wall": wall, "rss": rss, "count": count, "code": result.returncode}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo")
    parser.add_argument("--build", default=str(ROOT / "build/release"))
    parser.add_argument("--runs", type=int, default=3)
    parser.add_argument("--no-interact", action="store_true")
    args = parser.parse_args()
    env = {**os.environ, "QT_QPA_PLATFORM": "offscreen", "QT_QUICK_BACKEND": "software",
           "QT_QPA_PLATFORMTHEME": "", "GITNAGA_PERF": "1",
           "XDG_CONFIG_HOME": tempfile.mkdtemp(prefix="gitnaga-perf-")}
    runs = [run_once(Path(args.build) / "gitnaga", str(Path(args.repo).resolve()), not args.no_interact, env)
            for _ in range(args.runs)]
    for index, run in enumerate(runs):
        marks = "  ".join(f"{name.split(': ')[1]}={run['marks'].get(name, float('nan')):.0f}" for name in MILESTONES)
        slow = ", ".join(f"{k} {v:.1f}" for k, v in sorted(run["slow"].items(), key=lambda kv: -kv[1]))
        print(f"run {index}: commits={run['count']} {marks} wall={run['wall']}s rss={run['rss']}MB exit={run['code']}")
        print(f"        slowest GUI blocks (ms): {slow or 'none over budget'}")
    def median(key):
        values = [r["marks"][key] for r in runs if key in r["marks"]]
        return statistics.median(values) if values else float("nan")
    print("median: " + "  ".join(f"{name}={median(name):.0f}ms" for name in MILESTONES)
          + f"  rss={statistics.median(r['rss'] for r in runs)}MB")
    return 0 if all(r["code"] == 0 for r in runs) else 1


if __name__ == "__main__":
    sys.exit(main())
