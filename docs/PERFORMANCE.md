# Performance

Measured on the 1,000,000-commit fixture (`scripts/fixture-zoo.sh <dir> --big 1000000`,
five long-lived branches), Release build, offscreen, with `scripts/perf_bench.py`
(median of two runs). Times are milliseconds from the open request.

| Case | First rows painted | Fully loaded | Peak RSS | Slowest GUI block |
|------|-------------------:|-------------:|---------:|------------------:|
| Foundation, no commit-graph file | 3 906 | 7 804 | 835 MB | 42 ms |
| With commit-graph file | 133 | 8 099 | 822 MB | 57 ms |
| 64-bit row index (current) | 152 | 8 022 | 901 MB | 14 ms |

What decides the numbers:

- **First rows.** `git log --topo-order` reads the whole history before it
  prints anything unless the repository has a commit-graph file. GitNaga
  writes one in the background (`git commit-graph write --reachable`, the cache
  `git gc` maintains anyway) after loading a history of 20 000 commits or more
  that lacks it. The first open of such a repository waits ~4 s; every later
  open paints in ~150 ms.
- **Streaming.** History arrives in 4 000-commit chunks. The row index is
  keyed by the first 64 bits of each object id, so growing it to a million
  entries rehashes integers rather than strings; no chunk blocks the GUI
  thread for longer than a frame. (A full-id check on lookup keeps the
  vanishingly rare prefix collision correct.)
- **Hover and selection.** Emphasis sweeps ancestry only as far as the
  viewport (`src/graph_emphasis.cpp`), so hovering the top row of a million
  commits costs the visible rows, not the history.
- **The lane column** follows the widest row on screen, computed from the
  visible rows only.
- **The uncommitted row.** Row positions are stored relative to the history,
  so the uncommitted row appearing or disappearing renumbers nothing.

`scripts/acceptance.py --huge` opens the million-commit fixture in the real
window, then selects, hovers and scrolls deep into it.
