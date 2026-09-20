# Release Workflow

GitNaga releases are cut by `commit-and-tag-version`. The tool owns the version
number, the changelog, the release commit, and the tag. A release is one command
plus a push. Nothing else about the version is edited by hand.

## Version surfaces

`commit-and-tag-version` reads and writes every declared surface through
`.versionrc.js`. If a surface is not listed there, a release will leave it
behind; that is the failure this config exists to prevent.

| Surface | File | Updater | Role |
|---------|------|---------|------|
| Build and application version | `cmake/GitNagaVersion.cmake` | `cmakeUpdater` | The version CMake reads into `project()` and `GITNAGA_VERSION` |
| AppStream release history | `packaging/io.gitnaga.GitNaga.metainfo.xml` | `appstreamUpdater` | The `<releases>` list software centers display |

Rules:

- **Never** edit `CHANGELOG.md`, `cmake/GitNagaVersion.cmake`, or the AppStream
  `<releases>` block by hand. The next release overwrites or re-derives them.
- `packageFiles` names where the current version is read (`cmake/GitNagaVersion.cmake`).
- `bumpFiles` names every file the next version is written to (both surfaces).
- `docs`, `build`, `refactor`, `test`, `ci`, `perf`, and `revert` commits are
  mapped to changelog sections; `chore` is hidden. The commit type you choose
  determines what the changelog records, so use the correct conventional type.

## Commit convention

The changelog is generated from conventional commits since the previous tag:

```text
type(scope): imperative summary
```

A `feat` produces a minor bump, a `fix` a patch bump. A breaking change marker
(`!` or `BREAKING CHANGE:`) produces a major bump. Scope is optional. Write the
body for the reader of the release notes, not for the diff.

While the major version is `0`, the conventional preset applies pre-1.0 rules:
a feature is a patch bump and a breaking change is a minor bump. This is the
tool's default and the workflow does not override it. Force a specific version
with `--release-as` only when a target version is chosen deliberately.

## Preconditions

1. The branch is the one that should carry the release, normally `main`.
2. Every commit to release is committed. `make release-check` refuses a dirty
   tree.
3. `make pre-release` passes on the exact tree.

## Cutting a release

```bash
make release-dry     # preview the computed version and changelog, writes nothing
make release         # pre-release gate, then version + changelog + commit + tag
make release-push    # push branch and tag to every remote, then verify
```

`make release` runs, in order:

1. `make pre-release`: release build, full `ctest` suite, and `qmllint` over
   every QML file.
2. `scripts/release-config-check.sh`: verifies the config declares every
   surface, the surfaces exist, the AppStream latest release matches the build
   version, and the tree is clean.
3. `npx commit-and-tag-version`: computes the next version from the commits,
   rewrites both surfaces and `CHANGELOG.md`, creates the release commit, and
   creates the annotated tag `vX.Y.Z`.

The release commit is exactly the two surfaces plus `CHANGELOG.md`, subject
`chore(release): vX.Y.Z`, tagged `vX.Y.Z`. If a surface is missing from that
commit, the config is wrong: fix `.versionrc.js` and regenerate. Do not amend
around a missing surface, and never move the tag by hand.

To force a specific version, use the manual escape hatch and accept that it
bypasses the gate:

```bash
npx commit-and-tag-version --release-as 0.2.0
```

## Publishing and proof

`make release-push` pushes the branch and the tag, then proves the result.
GitHub uses the configured credential helper. The local Forgejo push uses a
one-use `write:repository` token generated through the `operate-local-forgejo`
helpers when they are available, and falls back to ambient credentials
otherwise. A token is never written to disk, a remote URL, or the credential
cache.

`scripts/verify-release.sh` requires, for every remote:

- the branch head equals the local head;
- the tag object equals the local tag object;
- the tag peel target equals the local tag target;
- `git rev-list --left-right --count branch...remote` returns `0 0`.

It exits non-zero on any mismatch. Run it alone with `make release-verify`, or
against a subset with `REMOTES="origin" make release-verify`.

## Continuous integration

The gate for a release is `make pre-release`. It is intentionally local: the
local Forgejo runners provide `rust`, `ruby`, and `python` images only, and
there is no Qt 6 toolchain runner, so a hosted build job would fail for a
reason unrelated to the change. Do not add a workflow that cannot run.

## Rollback

A release that has not been pushed is discarded by deleting the local tag and
resetting the release commit:

```bash
git tag -d vX.Y.Z
git reset --hard HEAD~1
```

A release that has been pushed is not rewritten. Cut a follow-up patch release
with `make release`; the changelog records the correction as a `fix`.
