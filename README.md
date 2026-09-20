# GitNaga

<p align="center">
  <img src="docs/history.png" alt="GitNaga showing a commit history with the commit graph, changed files, and a colored diff" width="920">
</p>

A native Linux Git history and diff viewer built with C++23 and Qt 6 QML. GitNaga
draws the commit graph with its own Qt Quick scene-graph item, lists commits and
refs in dense recycled delegates, and renders per-file diffs with syntax-aware
colors. It talks to Git directly and needs no daemon, no library wrapper, no
account, and no telemetry.

## Highlights

- **Real Git, no bindings.** History, refs, and diffs come from the `git`
  executable, so GitNaga shows exactly what Git stores. Merge topology and
  revision walks are read from machine-readable plumbing, not re-derived.
- **A graph that scales.** The commit graph is a custom batched `QQuickItem`
  that draws lanes in one scene-graph pass while text and ref labels live in
  recycled QML delegates.
- **Responsive by construction.** Every Git command runs off the GUI thread.
  Stale results are dropped by generation counter, and `.git` metadata is
  watched so the view refreshes when the repository changes underneath it.
- **A desktop frame, not a web page.** Menu bar, compact toolbar, split panes,
  dense lists, keyboard actions, and a status bar. The palette is dark navy and
  every corner is square.

## Screenshots

<p align="center">
  <img src="docs/empty-state.png" alt="GitNaga with no repository open, offering to open a local Git working tree or bare repository" width="920">
</p>

## Build and run

GitNaga needs a C++23 compiler, CMake 3.31 or newer, Ninja, and Qt 6.9.1 or
newer with the Quick, QuickControls2, and Concurrent modules.

```bash
make            # show the available targets
make build      # configure and build the debug preset
make run REPO=/path/to/repository
```

Without Make, the underlying CMake presets are the same:

```bash
cmake --preset dev
cmake --build --preset dev
./build/dev/gitnaga /path/to/repository
```

Passing a repository path is optional. With no argument, GitNaga opens on an
empty state and waits for **File > Open Repository**.

## Verify

```bash
make test       # build and run the ctest suite
make lint       # lint every QML file with qmllint
make ci-local   # configure, build, test, and lint in one pass
```

The integration test runs against a real scratch repository and covers
discovery, merge topology, commit inspection, and diff output. A source-size
contract rejects any source, QML, or test file at or above 300 lines.

## Layout

| Path | Contents |
|------|----------|
| `src/git_client.*` | Git discovery, history, inspection, and diff execution |
| `src/repository_controller.*` | Asynchronous repository and selection state for QML |
| `src/commit_graph_item.*` | Batched scene-graph renderer for the commit graph |
| `src/*_model.*` | Commit, changed-file, and diff-line list models |
| `qml/` | Application window, history pane, inspector, and shared controls |
| `tests/` | Smoke test and real-Git integration test |
| `cmake/` | Version surface and the source-size contract |

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
