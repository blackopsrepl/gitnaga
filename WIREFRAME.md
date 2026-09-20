# GitNaga Wireframe

A precise, implementation-linked specification of the GitNaga interface. Every
number in this document is read from the current source. When the source and
this document disagree, the source wins and this document is a bug.

- Shell: `qml/Main.qml`
- Graph pane: `qml/GraphPane.qml`
- References pane: `qml/ReferencesPane.qml`
- Review pane: `qml/InspectorPane.qml`
- Graph item: `src/commit_graph_item.*`, `src/commit_graph_paint.cpp`
- Graph geometry: `src/graph_geometry.*`
- Lane palette: `src/graph_palette.hpp`
- Operations menu: `qml/CommitMenu.qml`
- Dialogs: `qml/PromptDialog.qml`, `qml/ConfirmDialog.qml`
- Controls: `qml/NagaButton.qml`, `qml/NagaIconButton.qml`, `qml/NagaGlyph.qml`
- Empty state: `qml/EmptyState.qml`

## 1. Design rules (hard constraints)

1. **Square corners.** No `radius:` anywhere in `qml/`. Every rectangle, button,
   popup, and badge has square corners. This is checked by convention and is
   visible in the tokens below.
2. **300-line cap.** Every `src/*.cpp`, `src/*.hpp`, `qml/*.qml`, and
   `tests/*.cpp` file must be strictly under 300 lines. Enforced by
   `cmake/CheckSourceSize.cmake` as the `source_size_contract` test.
3. **Native Qt only.** C++23 and Qt 6 QML. No web engine, no Electron, no
   libgit2, no new runtime dependencies.
4. **GUI thread is never blocked.** All Git work runs on `QtConcurrent` workers
   and returns through `QFutureWatcher`.
5. **No duplicate views.** The centre graph already lists commits with message,
   author, time, and refs. Sidebars must carry information the centre does not.

## 2. Window shell

Default size `1480 x 900`. Minimum size `980 x 620`. Window `color` is
`#080b12`. Title is `<repositoryName> · GitNaga`, or `GitNaga` when no
repository is open.

```
+--------------------------------------------------------------------------+
| File  Repository  View                                  (native menu bar) |
+--------------------------------------------------------------------------+
| [Open Repository…] | gitnaga  ⎇ main        [References][Review](o)[Refresh]|  A
+--------------------------------------------------------------------------+
|            |                                   |                          |
| References |           Commit graph            |          Review          |  B
|  (left)    |            (centre)               |          (right)         |  B
|            |                                   |                          |
+--------------------------------------------------------------------------+
| /srv/lab/tools/gitnaga                            9 commits               |  C
+--------------------------------------------------------------------------+
```

```
+--------------------------------------------------------------------------+
| <error bar, only when errorMessage is not empty and a repo is open>       |  D
+--------------------------------------------------------------------------+
```

| Region | Element | Size | Notes |
|--------|---------|------|-------|
| A | `header: ToolBar` | `height: 40` | Raised `#141b2a`, bottom border `#222c40` |
| B | `SplitView` | fills remaining height | Hidden while no repository is open |
| C | `footer: Rectangle` | `height: 26` | Raised `#141b2a`, top border `#222c40` |
| D | error bar | `implicitHeight + 14` | `#5c1f28` fill, `#a84a55` border, white text |

### 2.1 Toolbar (region A), left to right

| Control | Width rule | State |
|---------|-----------|-------|
| `Open Repository…` button | `NagaButton`, `implicitHeight: 26` | Always enabled |
| `ToolSeparator` | 1 px | Static |
| Repository name label | `Layout.maximumWidth: 240`, elide middle | `repositoryName` or `No repository open` |
| Branch label | implicit | Visible only when `currentBranch` is non-empty, text `⎇ <branch>`, color `#34d399` |
| Spacer | `Layout.fillWidth: true` | Pushes the rest right |
| `References` toggle | `implicitHeight: 26` | `active` when the pane is visible |
| `Review` toggle | `implicitHeight: 26` | `active` when the pane is visible |
| `BusyIndicator` | `22 x 22` | Running only while `repository.busy` |
| `Refresh` button | `implicitHeight: 26` | Enabled when a repo is open and not busy |

### 2.2 Footer (region C), left to right

| Field | Rule | Color |
|-------|------|-------|
| Repository path | `Layout.fillWidth: true`, elide middle, `font.pixelSize: 11` | `#8590a3` |
| Operation message | Visible while `operationMessage` is non-empty, elide right | `#4ade80` on success, `#ff7d8b` on failure |
| Commit count | `<commits.count> commits`, `font.pixelSize: 11` | `#8590a3` |

Margins are `8` on both sides, `spacing: 10`.

## 3. Three-pane layout (region B)

A horizontal `SplitView`. Visibility of the outer panes is driven by
`referencesVisible` and `reviewVisible` (both default `true`). The graph pane
has `SplitView.fillWidth`, so it absorbs freed space when a sidebar closes.

| Pane | `preferredWidth` | `minimumWidth` | Visible when |
|------|------------------|----------------|--------------|
| References | `window.width * 0.24` | `200` | `referencesVisible` |
| Commit graph | fills remainder | `380` | repo open |
| Review | `window.width * 0.38` | `320` | `reviewVisible` |

At the default `1480` width with both sidebars open the centre graph receives
approximately the remaining third.

### 3.1 Visibility states

```
Both open            References only      Review only          Graph only
+--+--------+-----+  +--+---------------+  +--------+-----+     +---------------+
|R | Graph  | Rev |  |R |   Graph       |  | Graph  | Rev |     |     Graph     |
|  |        |     |  |  |               |  |        |     |     |               |
+--+--------+-----+  +--+---------------+  +--------+-----+     +---------------+
Ctrl+1 toggles R     Ctrl+2 toggles Rev
```

## 4. References pane (left)

Header `34` px, filter `32` px, then a scrolling column of sections. Rows are
`26` px; section headers are `24` px.

```
+--------------------------------------+
| References                        ✕  |  34
+--------------------------------------+
| [ Filter references                ] |  32
+--------------------------------------+
| BRANCHES  1                          |  24   section header
|   * main                     ed040828|  26   row
| REMOTES  2                           |  24
|   * github/main              ed040828|  26
|   * origin/main              ed040828|  26
| TAGS  0                              |  24
+--------------------------------------+
```

Row anatomy: a `8 x 8` colour swatch at `leftMargin: 12` using the lane colour
of the referenced commit, the reference name at `leftMargin: 28` (elide right,
`font.pixelSize: 12`), and the short oid at `rightMargin: 10` (monospace,
`font.pixelSize: 10`, colour `#8590a3`). Hover fills the row with `#1a2233`.

Sections are derived from the refs Git stores: local branches (no prefix),
remotes (`⇄ ` prefix), and tags (`# ` prefix). Each section header shows the
filtered count. The filter box matches case-insensitively against the name.

Right-click a row opens a static `Menu`:

| Item | Visible for |
|------|-------------|
| `Checkout <name>` | local |
| `Delete branch <name>…` (confirmed) | local |
| `Show commit <shortOid>` | all |
| `Checkout commit` | all |
| `Copy name` | all |

Left-click selects the referenced commit through `repository.selectOid(oid)`.

## 5. Commit graph pane (centre)

Header `34` px. The graph item fills the pane; a non-interactive label list is
overlaid to the right of the lane column.

```
+----------------------------------------------------------------------+
| Commit graph  9 commits                    [−] 100% [+] [⟲]          |  34
+----------------------------------------------------------------------+
|   o   chore(release): 0.1.0          [main][⇢ github/main][⇢ origin] |
|   |   ed040828  Vittorio Distefano  ·  41m ago                        |
|   o   fix(release): point changelog links at the published repository|
|   |   0fca445a  Vittorio Distefano  ·  42m ago                        |
|   o   docs: add a project README with interface screenshots          |
|   |   03ace8cb  Vittorio Distefano  ·  42m ago                        |
|  ...                                                                  |
+----------------------------------------------------------------------+
      ^ lane column            ^ label overlay starts at laneWidth + 16
```

### 5.1 Header controls

| Control | Action |
|---------|--------|
| `−` | `graph.zoomOut()` (divide by 1.18), disabled at `zoom <= 0.46` |
| Zoom readout | `Math.round(zoom * 100) + "%"`, `Layout.preferredWidth: 34` |
| `+` | `graph.zoomIn()` (multiply by 1.18), disabled at `zoom >= 2.79` |
| `⟲` | `graph.resetZoom()` |

### 5.2 Geometry (base values at zoom 1.0)

Rows are dense and nodes are sized to the row, not to the lane.

| Quantity | Value | Source |
|----------|-------|--------|
| Row height | `36 * zoom` | `GraphStyle.rowHeight` |
| Lane spacing | `22 * zoom` | `GraphStyle.laneSpacing` |
| Left padding | `26` (fixed) | `GraphStyle.leftPadding` |
| Lane x | `26 + lane * 22 * zoom` | `laneX()` |
| Lane area width | `26 + (maxLane + 1) * 22 * zoom` | `laneAreaWidth()` |
| Zoom range | `0.45 .. 2.8` | `commit_graph_item.cpp` |
| Edge shadow pen | `3.2 * zoom`, colour `#04060b` alpha 110 | `paintEdges()` |
| Edge pen | `2.2 * zoom`, lane-colour gradient, round joins | `paintEdges()` |
| Node disc radius | `clamp(8.0 * zoom, 4.0, 16.0)`, merge `* 1.2` | `paintNode()` |
| Node ring | at disc `+2.4 * zoom`, pen `1.8 * zoom` | `paintNode()` |
| Row band alpha | idle `26`, hover `38`, selected `56`, dimmed `10` | `paint()` |
| Dim factor | unrelated commits at 50% alpha | `paintEdges()`, `paintNode()` |

### 5.2.1 Edge routing

Edges use **elbow routing**, not a full-row S-curve. From the child node the
path holds vertical, takes a rounded quarter turn onto the row boundary, runs
horizontally, takes a second rounded turn, then holds vertical into the parent
node. The turn radius is `min(|dx| / 2, rowHeight * 0.42, 12 * zoom)`.

This is what keeps a lane change reading as a clean branch rather than a
diagonal streak. `edgesFor()` samples the routed polyline by arc length so the
point count stays stable and evenly spaced.

### 5.3 Highlighting

Hovering or selecting a commit computes its **ancestry** by walking parents
through an oid to row map. Commits that are not ancestors of the active commit
are drawn at 50% alpha; the active commit and its ancestry stay fully opaque.
This is the "trace this line of history" behaviour, and it is the reason
hovering is useful: the branch you are inspecting comes forward and everything
else recedes. Setting alpha floors is unnecessary
because the dim is a single constant in `commit_graph_paint.cpp`.

### 5.4 Avatars

Each node carries the author avatar inside its disc, ringed by the branch
colour. Avatars are resolved offline with no network access:

1. `<cache>/avatars/<sha256(email)>.png` is used when present, so real photos
   can be dropped in. The cache directory is the application cache location
   (`~/.cache/GitNaga/avatars` by default).
2. Otherwise a deterministic **monogram** is drawn: the author initials on a
   colour from a curated muted palette, selected by the email hash. The same
   author always renders the same avatar, and the muted disc keeps the
   branch-coloured ring as the dominant signal. Block identicons were tried and
   are illegible at this node size.

When a commit has no ancestors in the dimmed set the avatar is drawn at 50%
opacity with the rest of the node.

### 5.5 Paint order (back to front)

1. Per-row branch band, full pane width, colour = lane colour of the row.
2. Edges, each with a dark shadow pass then a gradient main pass.
3. Commit nodes: the author avatar clipped to the disc, or a solid
   lane-colour disc when no avatar is available, then a hairline dark rim, one
   coloured ring with a background gap, and a soft glow on the selected node.

> The disc is a **flat** fill, either the avatar or the lane colour. Earlier
> revisions used a light radial gradient with a specular highlight, which read
> as a pale bubble; flat fill with a crisp ring is the intended look.

### 5.6 Label overlay

A `ListView` with `interactive: false`, `enabled: false`, `contentY` bound to
`graph.contentY`, so it never steals pointer events. `x = min(width - 150,
laneWidth + 16)`.

Delegate (`height = graph.effectiveRowHeight`, 11 px subject over 9 px meta,
`spacing: 1`):

```
Row 1:  <subject, elide right, width = parent - chips - 8>  [chip][chip][chip]
Row 2:  <shortOid>  <author>  ·  <relativeDate>
```

Chips are 14 px tall with 9 px text and show at most 3 refs. Chip colours: local `#1c2140` with border `#5b4bb8`,
remote `#262d3d` with border `#3a4560`, tag `#3a2f17` with border `#7a5f1f`.
`shortOid` uses `#a78bfa`; author and date use `#78839a`. The selected row uses
white subject text and `Font.DemiBold`.

### 5.7 Pointer model

| Gesture | Effect |
|---------|--------|
| Hover | Set `hoveredRow` and recompute the ancestry highlight immediately; ignored while a pan drag is in progress |
| Left click | Emit `commitClicked(row)`; the shell calls `selectCommit(row)`, which re-highlights its ancestry |
| Left drag (> 4 px) | Pan: `contentY = pressContentY - delta` |
| Wheel | Scroll by one row height per 120 units of `angleDelta` |
| Ctrl + wheel | Zoom by `pow(1.0015, delta)`, anchored under the cursor |
| Right click | Emit `contextRequested(row, oid, globalPos)`; a row opens the operations menu, empty space opens a refresh menu |

A row is only reported when the pointer is inside the real row range: the
pointer in the empty space below the last commit reports no row, so no ring or
highlight is applied and a click there does nothing.

Scroll range is `0 .. max(0, rowCount * effectiveRowHeight - height)`.
`ensureVisible(row)` scrolls the minimum amount so the row is inside the
viewport; the shell calls it whenever the selection changes.

### 5.8 Menus and the repository dialog

The menu bar uses the application's own menu components rather than the
default control sizing, which was too narrow and let labels run under their
shortcut column.

| Component | Role |
|-----------|------|
| `NagaMenu` | Dark popup: `#12161f` fill, `#2b3242` border, `4` px padding |
| `NagaMenuItem` | `28` px row, minimum `240` px wide, `12` px label, right-aligned shortcut column, drawn check mark for checkable actions, `#1e2634` highlight |
| `NagaMenuSeparator` | 1 px line in `#242c3b` |

Menu actions use literal key sequences (`"Ctrl+O"`) so the shortcut column can
display them; `StandardKey` values stringify to their enum numbers and are not
usable as labels. The checkable sidebar toggles draw their own check mark
because the default indicator overlaps the label when the content is custom.

`OpenRepositoryDialog` replaces the platform folder dialog, which ignored the
application palette and rendered a light toolbar with unreadable text against
the dark window. It is a modal `Dialog` with:

- a title row with a close button;
- an `Up` button, an editable path field, and a `Go` button;
- a directory list built from `repository.directories(path)`, sorted, hidden
  entries excluded, with a marker for entries that are repositories;
- a footer showing whether the current directory is a repository, an
  `Open Repository` button enabled only for a repository, and `Cancel`.

Escape closes it. `repository.looksLikeRepository(path)` accepts both a `.git`
directory and a bare repository layout.

## 6. Operations menu (right click on a commit)

A `Popup`, not a `Menu`, so the item count can vary. Width `320`. Normal rows
are `26` px, separators are `9` px. Background `#12161f`, border `#2b3242`.
Hover fill `#1e2634`. Destructive labels use `#ff8f9c`; disabled labels use
`#5b6478`. It closes on Escape or on a press outside.

```
+--------------------------------------+
| Checkout a7908a89                    |
| Checkout branch feature              |
|--------------------------------------|
| Create branch here…                  |
| Create tag here…                     |
|--------------------------------------|
| Cherry-pick a7908a89 onto main       |
| Merge a7908a89 into main             |
| Revert a7908a89                      |
|--------------------------------------|
| Reset main to a7908a89 (soft)        |
| Reset main to a7908a89 (mixed)       |
| Hard reset main to a7908a89…         |  red
| Rebase main onto a7908a89…           |  red
|--------------------------------------|
| Delete branch feature…               |  red, one per local ref
|--------------------------------------|
| Copy commit hash                     |
| Refresh                              |
+--------------------------------------+
```

The `Checkout branch` and `Delete branch` groups repeat once per local branch
ref attached to the clicked commit. `Hard reset`, `Rebase`, and `Delete branch`
route through `ConfirmDialog`.

## 7. Review pane (right)

Header `34` px. A details block, then a horizontal splitter of file list and
diff.

```
+----------------------------------------------------------------------+
| Review                                                            ✕  |  34
+----------------------------------------------------------------------+
| feat(history): render the interactive commit graph                   |  details
| Vittorio Distefano  ·  Sunday, 20 September 2026 ...                 |  block
| <body, up to 3 lines>                                                |
| c653e391c36be16209e8ab783a2da4143a10de6d                            |
+----------------------------------------------------------------------+
| Changed files        | old new | diff                                 |
| M CMakeLists.txt     |  31  31 | ...                                  |
| A EmptyState.qml     |  34  34 | +qt_add_executable(gitnaga           |
| ...                  |         | ...                                  |
+----------------------------------------------------------------------+
```

| Element | Size |
|---------|------|
| Details block | `min(150, implicitHeight + 20)` when selected, else `62`; margins `10`, spacing `5` |
| Details title | `font.pixelSize: 15`, `Font.DemiBold`, wrap |
| Files column | `SplitView.preferredWidth: 200`, `minimumWidth: 150`, header `28` |
| Diff column | `SplitView.fillWidth`, `minimumWidth: 240` |
| Diff line row | `height: 22` |
| Diff line-number columns | two columns of `40` px, monospace `11`, colour `#5b6577`, `1` px divider |

Diff row background: addition `#12291f`, deletion `#331a20`, hunk `#182b3f`,
context transparent. Code colour: addition `#8fe6b8`, deletion `#ffadb6`, hunk
`#9fc2ff`, header `#77839a`, context `#cbd0dc`. File status letter colour:
added `#4ade80`, deleted `#ff7d8b`, otherwise `#fbbf24`.

## 8. Dialogs

| Dialog | Width | Behaviour |
|--------|-------|-----------|
| `PromptDialog` | `360` | Modal, `padding: 16`, one `TextField`, OK and Cancel standard buttons. Emits `submitted(string)` only when the trimmed value is non-empty. Carries `pendingOid` for the caller. |
| `ConfirmDialog` | `400` | Modal, `padding: 16`, wrapped message label, OK and Cancel. Emits `confirmed()`. |

## 9. Empty state and error bar

When `repositoryPath` is empty the `SplitView` is hidden and `EmptyState`
fills the window: a centred column `min(420, width - 48)` wide, `spacing: 10`,
with the title `No repository open`, the hint `Open a local Git working tree or
bare repository.`, an `Open Repository…` button, and the error message when
present in `#d84f5f`.

The error bar (region D) is visible only when a repository is open and
`errorMessage` is non-empty. It sits directly above the footer, spans the
width, and elides its text to one line.

## 10. Colour tokens

Shell tokens (`qml/Main.qml`), propagated to panes through the `colors`
property:

| Token | Value | Use |
|-------|-------|-----|
| `backgroundColor` | `#080b12` | Window and graph background |
| `panel` | `#0e1420` | Pane backgrounds and headers |
| `raised` | `#141b2a` | Toolbar and footer |
| `alternate` | `#1a2233` | Hover fill |
| `border` | `#222c40` | All dividers and outlines |
| `text` | `#e8ebf2` | Primary text |
| `muted` | `#8590a3` | Secondary text |
| `accent` | `#34d399` | Branch label, focus, success |
| selection text | `#eafff6` | Selection rings |
| destructive | `#ff8f9c` | Dangerous menu entries |
| error fill/border | `#5c1f28` / `#a84a55` | Error bar |

Lane palette (`src/graph_palette.hpp`), selected by `lane % 10`:

| Index | Hex | Index | Hex |
|-------|-----|-------|-----|
| 0 | `#a970ff` | 5 | `#e05252` |
| 1 | `#3d91f4` | 6 | `#45c5e0` |
| 2 | `#e350b0` | 7 | `#f07a3d` |
| 3 | `#4fbf67` | 8 | `#b78af5` |
| 4 | `#e8c545` | 9 | `#67d9a0` |

Helper derivations: `lighten` mixes toward `#ffffff`, `darken` mixes toward
`#05070c`, and `withAlpha` only changes the alpha channel.

## 11. Keyboard map

| Shortcut | Action |
|----------|--------|
| `Ctrl+O` | Open repository |
| `Ctrl+R` / `StandardKey.Refresh` | Refresh |
| `Ctrl+Q` | Quit |
| `Ctrl+1` | Toggle References sidebar |
| `Ctrl+2` | Toggle Review sidebar |
| `Ctrl+=` | Zoom graph in |
| `Ctrl+-` | Zoom graph out |
| `Ctrl+0` | Reset graph zoom |
| `Alt+Up` | Select previous commit |
| `Alt+Down` | Select next commit |

## 12. State matrix

| State | Toolbar | Region B | Footer | Busy |
|-------|---------|----------|--------|------|
| No repository | Open enabled, Refresh disabled | EmptyState | Path empty, `0 commits` | No |
| Loading | Refresh disabled | Prior content or empty | Prior content | Yes |
| Loaded | Refresh enabled | SplitView with panes | Path and count | No |
| Operation running | Refresh disabled | Unchanged | Operation message appears | Yes |
| Operation failed | Refresh enabled | Unchanged | Message in `#ff7d8b` | No |
| Repository error | As loaded | As loaded | As loaded | No |
| Selection active | As loaded | Selected row band and rings, Review populated | As loaded | No |

## 13. Data binding map

Controller properties consumed by QML:

| Binding | Type | Source |
|---------|------|--------|
| `repository.commits` | `CommitModel` | roles `oid`, `shortOid`, `parents`, `author`, `authoredAt`, `relativeDate`, `subject`, `refs`, `lane`, `laneCount` |
| `repository.changedFiles` | `FileChangeModel` | roles `status`, `path`, `oldPath`, `fileName`, `directory` |
| `repository.diffLines` | `DiffLineModel` | roles `kind`, `text`, `oldLine`, `newLine` |
| `repository.repositoryPath` / `repositoryName` / `currentBranch` | `QString` | Repository metadata |
| `repository.loading` / `busy` | `bool` | Work in flight |
| `repository.errorMessage` / `operationMessage` | `QString` | Diagnostics |
| `repository.selectedRow` | `int` | Selected commit index |
| `repository.selectedOid` / `selectedSubject` / `selectedAuthor` / `selectedDate` / `selectedBody` | `QString` | Selected commit details |
| `repository.references` | `QVariantList` | maps with `name`, `kind`, `oid`, `shortOid`, `subject`, `color` |

Controller invokables called by QML: `openRepository`, `openRepositoryPath`,
`refresh`, `selectCommit`, `selectOid`, `selectFile`, `copyToClipboard`,
`checkoutCommit`, `checkoutBranch`, `createBranch`, `deleteBranch`,
`createTag`, `cherryPick`, `revertCommit`, `mergeCommit`, `resetTo`,
`rebaseOnto`.

Controller signals observed by QML: `repositoryChanged`, `loadingChanged`,
`busyChanged`, `errorChanged`, `operationMessageChanged`,
`operationFinished(ok, message)`, `selectionChanged`, `referencesChanged`.

Graph item API: properties `model`, `contentY`, `baseRowHeight`, `zoom`,
`selectedRow`, `hoveredRow`, `headOid`, `effectiveRowHeight`, `laneWidth`,
`contentHeight`, `maxContentY`; invokables `zoomIn`, `zoomOut`, `resetZoom`,
`scrollToRow`, `ensureVisible`; signals `commitClicked(row)`,
`contextRequested(row, oid, globalPosition)` and the change notifications for
each property.

## 14. Accessibility surface

Qt publishes the window as `GitNaga` with roles for the menu bar, toolbar
push buttons (`Open Repository…`, `References`, `Review`, `Refresh`), the pane
close buttons (`✕`), and the zoom buttons (`−`, `+`, `⟲`). This is the surface
agents drive with `lumen accessibility` and `lumen click`, and the surface the
verification loop uses to confirm state changes after an action.

## 15. Not implemented (explicit)

- Remote avatars (Gravatar, GitHub). Avatars are local or generated; the application makes no network calls.
- Staging, committing, and a working-copy view. GitNaga is a history and review
  tool.
- Merge conflict resolution, interactive rebase editing, and stash management.
- Rounded corners, by design rule.
