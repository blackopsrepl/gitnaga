const versionUpdater = {
  readVersion(contents) {
    const match = contents.match(/set\(GITNAGA_VERSION "([^"]+)"\)/);
    if (!match) throw new Error("GITNAGA_VERSION not found");
    return match[1];
  },
  writeVersion(contents, version) {
    return contents.replace(
      /set\(GITNAGA_VERSION "[^"]+"\)/,
      `set(GITNAGA_VERSION "${version}")`,
    );
  },
};

module.exports = {
  packageFiles: [{ filename: "cmake/GitNagaVersion.cmake", updater: versionUpdater }],
  bumpFiles: [{ filename: "cmake/GitNagaVersion.cmake", updater: versionUpdater }],
  tagPrefix: "v",
  releaseCommitMessageFormat: "chore(release): {{currentTag}}",
  commitUrlFormat: "https://github.com/blackopsrepl/gitnaga/commit/{{hash}}",
  compareUrlFormat: "https://github.com/blackopsrepl/gitnaga/compare/{{previousTag}}...{{currentTag}}",
};
