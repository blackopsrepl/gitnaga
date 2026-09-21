# Changelog

All notable changes to this project are documented here. Entries are
generated from conventional commits by commit-and-tag-version; do not edit
this file by hand.

## [0.1.14](https://github.com/blackopsrepl/gitnaga/compare/v0.1.13...v0.1.14) (2026-09-21)

### Code Refactoring

* **graph:** extract the commit label overlay ([4c32e64](https://github.com/blackopsrepl/gitnaga/commit/4c32e64209bfcadb29b26d40adb85af9dadac903))

### Tests

* **palette:** split the colour contracts into their own test ([402e0ea](https://github.com/blackopsrepl/gitnaga/commit/402e0ea077c2a7e8fe94b1b68c33521b4e394c56))

## [0.1.13](https://github.com/blackopsrepl/gitnaga/compare/v0.1.12...v0.1.13) (2026-09-21)

### Bug Fixes

* **ui:** restore the pane backgrounds after the theme refactor ([d762646](https://github.com/blackopsrepl/gitnaga/commit/d762646320978df917c3291b5ce781a8afdeeb3d))

## [0.1.12](https://github.com/blackopsrepl/gitnaga/compare/v0.1.11...v0.1.12) (2026-09-21)

## [0.1.11](https://github.com/blackopsrepl/gitnaga/compare/v0.1.10...v0.1.11) (2026-09-21)

### Features

* **graph:** give branch lines stable, well-separated neon colours ([22beee7](https://github.com/blackopsrepl/gitnaga/commit/22beee75031d3756926c00de8e9d5019346f767b))
* **ui:** tint commit badges with their branch colour ([8b96e02](https://github.com/blackopsrepl/gitnaga/commit/8b96e023890b0ed443f72d042b7d887b18626660))

### Bug Fixes

* **graph:** carry the edge colour fields the palette commit referenced ([2e4774f](https://github.com/blackopsrepl/gitnaga/commit/2e4774f5fbacc9a021c8319aba390af33c421078))

## [0.1.10](https://github.com/blackopsrepl/gitnaga/compare/v0.1.9...v0.1.10) (2026-09-21)

### Bug Fixes

* **graph:** keep the selected commit emphasised while hovering ([2e74150](https://github.com/blackopsrepl/gitnaga/commit/2e7415002b8d947d7e881c1db996b09bee87f21f))

## [0.1.9](https://github.com/blackopsrepl/gitnaga/compare/v0.1.8...v0.1.9) (2026-09-21)

### Code Refactoring

* **wip:** model uncommitted work as an ephemeral commit ([81f09d8](https://github.com/blackopsrepl/gitnaga/commit/81f09d80b56a03123dead11eb523e6731e8fde01))

## [0.1.8](https://github.com/blackopsrepl/gitnaga/compare/v0.1.7...v0.1.8) (2026-09-21)

### Features

* **graph:** show work in progress as a synthetic pending row ([e3a71b0](https://github.com/blackopsrepl/gitnaga/commit/e3a71b09855a357e0efe6565abccce4cc4cb5fac))

## [0.1.7](https://github.com/blackopsrepl/gitnaga/compare/v0.1.6...v0.1.7) (2026-09-21)

### Features

* **graph:** add a slim scrollbar and collapse labels at low zoom ([1adb5be](https://github.com/blackopsrepl/gitnaga/commit/1adb5bedeff998afe8a8cc5968dca98108d43eaf))
* **ui:** draw folder and refresh vector icons for the toolbar ([7e1139e](https://github.com/blackopsrepl/gitnaga/commit/7e1139e73db237893089cf74f5aa92b332e93746))
* **ui:** drop the classic menu bar ([0180b0f](https://github.com/blackopsrepl/gitnaga/commit/0180b0fc42a68d09e8fe977b19ddafa74caf62d2))
* **ui:** switch branches by double click with a visible HEAD marker ([951710c](https://github.com/blackopsrepl/gitnaga/commit/951710ca4b55cddb11a0234d064b64052405a2ea))

### Bug Fixes

* **controller:** honour a newly requested repository path ([38b629b](https://github.com/blackopsrepl/gitnaga/commit/38b629b279874686ffc1128ecef2ea78acafb374))

## [0.1.6](https://github.com/blackopsrepl/gitnaga/compare/v0.1.5...v0.1.6) (2026-09-20)

### Bug Fixes

* **ci:** assert the C++23 language mode, not one compiler's macro value ([a7c005a](https://github.com/blackopsrepl/gitnaga/commit/a7c005aa9a715b0d4a7bd6bcc24979f4fffb2f5b))
* **ci:** quote the lint command so the workflow file parses ([26a53c3](https://github.com/blackopsrepl/gitnaga/commit/26a53c3114a3c8141cdd8214e2543cd72b64c608))

## [0.1.5](https://github.com/blackopsrepl/gitnaga/compare/v0.1.4...v0.1.5) (2026-09-20)

### Bug Fixes

* **graph:** rebuild the ancestry highlight when the row set changes ([fbea376](https://github.com/blackopsrepl/gitnaga/commit/fbea3764f483d84f661fac85195843b4a4bd83bf))

### Documentation

* describe avatars, menus, and the repository dialog ([bb3c6f7](https://github.com/blackopsrepl/gitnaga/commit/bb3c6f75f010893f6a15dd9b960cea3bdecf39d1))

## [0.1.4](https://github.com/blackopsrepl/gitnaga/compare/v0.1.3...v0.1.4) (2026-09-20)

### Features

* **graph:** draw per-author avatars on commit nodes ([e3a0733](https://github.com/blackopsrepl/gitnaga/commit/e3a0733228818de741808e104b7e773947a2340b))
* **ui:** style the menus and replace the platform folder dialog ([2251ae8](https://github.com/blackopsrepl/gitnaga/commit/2251ae81de697f87bd437e92327d63d57ec89b38))

### Bug Fixes

* **graph:** correct the hover triggers ([d1eb800](https://github.com/blackopsrepl/gitnaga/commit/d1eb800186aeeb5a631aadadee594dfd97b265db))

## [0.1.3](https://github.com/blackopsrepl/gitnaga/compare/v0.1.2...v0.1.3) (2026-09-20)

### Bug Fixes

* **build:** keep the commit graph item within the source size contract ([c35c187](https://github.com/blackopsrepl/gitnaga/commit/c35c1872559732b3068db5434ab31cc0d3ef9679))

### Build System

* add install and uninstall targets ([cc2eaa6](https://github.com/blackopsrepl/gitnaga/commit/cc2eaa687b8acf890ae6933f27681101c2f29757))

## [0.1.2](https://github.com/blackopsrepl/gitnaga/compare/v0.1.1...v0.1.2) (2026-09-20)

### Features

* **graph:** rebuild graph rendering around edges, ancestry, and density ([a27f8e4](https://github.com/blackopsrepl/gitnaga/commit/a27f8e4a7406adbb522bfe6c07cf26df79711d73))

### Bug Fixes

* **lint:** run the Qt 6 qmllint instead of the Qt 5 binary ([f564862](https://github.com/blackopsrepl/gitnaga/commit/f564862971adbc4d2a49276cd1956d79da2678cf))
* **ui:** strip mnemonic markers from action buttons ([522bb28](https://github.com/blackopsrepl/gitnaga/commit/522bb289e5ee8cd6303dd0a59bfa015ff3356db5))

### Documentation

* describe the hosted CI and add a build badge ([5dbac93](https://github.com/blackopsrepl/gitnaga/commit/5dbac934da8542ae104ac36bfa05f92cd233513d))
* refresh the graph screenshots ([857d4e3](https://github.com/blackopsrepl/gitnaga/commit/857d4e3dc9784cc33b15acf97175674f02c60939))

### Continuous Integration

* build, test, and lint on GitHub and Forgejo ([6a8664e](https://github.com/blackopsrepl/gitnaga/commit/6a8664e04b3ab77b77161ad0e611db237e129fd4))

## [0.1.1](https://github.com/blackopsrepl/gitnaga/compare/v0.1.0...v0.1.1) (2026-09-20)

### Features

* **graph:** rebuild the history view around an interactive commit graph ([cae6919](https://github.com/blackopsrepl/gitnaga/commit/cae6919686580857257696f7e6d90b627f98a395))

### Documentation

* add a methodical interface wireframe ([7c1d0f2](https://github.com/blackopsrepl/gitnaga/commit/7c1d0f2cddfa5261275b3af215d0545d61a42c53))
* document the graph-centric interface with new screenshots ([7a94712](https://github.com/blackopsrepl/gitnaga/commit/7a94712ee3ca860883c7b5a99a402b3dde249706))
* **release:** note the pre-1.0 bump rules ([e3c6a59](https://github.com/blackopsrepl/gitnaga/commit/e3c6a594360330f174f0c3bd2bc2974e4c0eb166))

### Build System

* **release:** establish the commit-and-tag-version release workflow ([ec21a66](https://github.com/blackopsrepl/gitnaga/commit/ec21a66a0343f9041ffc7558423763a04d17c9af))

## 0.1.0 (2026-09-20)

### Features

* **history:** render the interactive commit graph ([c653e39](https://github.com/blackopsrepl/gitnaga/commit/c653e391c36be16209e8ab783a2da4143a10de6d))
* **repository:** load Git history and commit details ([b5ee614](https://github.com/blackopsrepl/gitnaga/commit/b5ee614f2ec78f514f7b2f571a5afd3cdf7aee21))
* **ui:** adopt a traditional desktop frame ([397c277](https://github.com/blackopsrepl/gitnaga/commit/397c2776872bcbe6d2e1c2654ee95b347c03e67b))

### Bug Fixes

* **history:** highlight a commit row on hover ([883805e](https://github.com/blackopsrepl/gitnaga/commit/883805eb73fbed84152d2078c7a8f2f8e17eecd4))
* **release:** point changelog links at the published repository ([0fca445](https://github.com/blackopsrepl/gitnaga/commit/0fca445ae364cbf3a050ae41a2810a86376aa0c1))
