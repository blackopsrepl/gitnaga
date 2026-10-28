# Performance

Measured on the 1,000,000-commit fixture (`scripts/fixture-zoo.sh <dir> --big 1000000`,
five long-lived branches), Release build, offscreen, with `scripts/perf_bench.py`
(median of two runs). Times are milliseconds from the open request.

| Case | First rows painted | Fully loaded | Peak RSS | Slowest GUI block |
|------|-------------------:|-------------:|---------:|------------------:|
| Foundation, no commit-graph file | 3 906 | 7 804 | 835 MB | 42 ms |
| With commit-graph file | 133 | 8 099 | 822 MB | 57 ms (see below) |

What decides the numbers:

- **First rows.** `git log --topo-order` reads the whole history before it
  prints anything unless the repository has a commit-graph file. GitNaga
  writes one in the background (`git commit-graph write --reachable`, the cache
  `git gc` maintains anyway) after loading a history of 20 000 commits or more
  that lacks it. The first open of such a repository waits ~4 s; every later
  open paints in ~130 ms.
- **Hover and selection.** Emphasis sweeps ancestry only as far as the
  viewport (`src/graph_emphasis.cpp`), so hovering the top row of a million
  commits costs the visible rows, not the history.
- **The uncommitted row.** Row positions are stored relative to the history,
  so the uncommitted row appearing or disappearing renumbers nothing.
- **Known gap: growth stalls.** While a long history streams in, the row
  storage doubles a handful of times (five or six over a million rows); each
  doubling relocates every row loaded so far and blocks the GUI thread for
  25–55 ms. All other appends stay under 10 ms. Removing it needs chunked
  storage or a size estimate up front.
