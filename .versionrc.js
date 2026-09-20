// commit-and-tag-version configuration.
//
// This file is the single source of truth for the release workflow. It declares
// every version surface so a release cannot leave packaging metadata behind.
//
// Surfaces:
//   1. cmake/GitNagaVersion.cmake          the build and application version
//   2. packaging/..metainfo.xml            the AppStream release history
//
// Never hand-edit CHANGELOG.md or either surface. Run `make release`.

const cmakeUpdater = {
  readVersion(contents) {
    const match = contents.match(/set\(GITNAGA_VERSION "([^"]+)"\)/);
    if (!match) throw new Error("GITNAGA_VERSION not found in cmake/GitNagaVersion.cmake");
    return match[1];
  },
  writeVersion(contents, version) {
    if (!/set\(GITNAGA_VERSION "[^"]+"\)/.test(contents))
      throw new Error("GITNAGA_VERSION not found in cmake/GitNagaVersion.cmake");
    return contents.replace(
      /set\(GITNAGA_VERSION "[^"]+"\)/,
      `set(GITNAGA_VERSION "${version}")`,
    );
  },
};

const appstreamUpdater = {
  readVersion(contents) {
    const match = contents.match(/<release version="([^"]+)"/);
    return match ? match[1] : "0.0.0";
  },
  writeVersion(contents, version) {
    const date = new Date().toISOString().slice(0, 10);
    const entry = `<release version="${version}" date="${date}"/>`;
    if (!contents.includes("</component>"))
      throw new Error("malformed AppStream metainfo: no </component>");

    if (/<releases>/.test(contents)) {
      const first = contents.match(/<release version="([^"]+)"/);
      if (first && first[1] === version)
        return contents.replace(/<release version="[^"]+"( date="[^"]+")?\/>/, entry);
      return contents.replace(/<releases>/, `<releases>\n    ${entry}`);
    }
    return contents.replace(
      /<\/component>/,
      `  <releases>\n    ${entry}\n  </releases>\n</component>`,
    );
  },
};

module.exports = {
  header: [
    "# Changelog",
    "",
    "All notable changes to this project are documented here. Entries are",
    "generated from conventional commits by commit-and-tag-version; do not edit",
    "this file by hand.",
    "",
  ].join("\n"),
  types: [
    { type: "feat", section: "Features" },
    { type: "fix", section: "Bug Fixes" },
    { type: "perf", section: "Performance" },
    { type: "revert", section: "Reverts" },
    { type: "refactor", section: "Code Refactoring" },
    { type: "docs", section: "Documentation" },
    { type: "build", section: "Build System" },
    { type: "test", section: "Tests" },
    { type: "ci", section: "Continuous Integration" },
    { type: "chore", hidden: true },
  ],
  packageFiles: [{ filename: "cmake/GitNagaVersion.cmake", updater: cmakeUpdater }],
  bumpFiles: [
    { filename: "cmake/GitNagaVersion.cmake", updater: cmakeUpdater },
    { filename: "packaging/io.gitnaga.GitNaga.metainfo.xml", updater: appstreamUpdater },
  ],
  tagPrefix: "v",
  releaseCommitMessageFormat: "chore(release): {{currentTag}}",
  commitUrlFormat: "https://github.com/blackopsrepl/gitnaga/commit/{{hash}}",
  compareUrlFormat: "https://github.com/blackopsrepl/gitnaga/compare/{{previousTag}}...{{currentTag}}",
};
