# Changelog

All notable changes to this project are documented here. Entries are
generated from conventional commits by csemver; do not edit
this file by hand.

## [0.2.0](https://github.com/blackopsrepl/gitnaga/compare/v0.1.22...v0.2.0) (2026-10-28)


### ⚠ BREAKING CHANGES

* **core:** RepositorySnapshot no longer carries workInProgressOid; the uncommitted row is the WORKTREE pseudo-oid.

### Features

* **conflicts:** parse conflict markers into resolvable blocks ([ce73832](https://github.com/blackopsrepl/gitnaga/commit/ce738322313960791d5beee85f43bac928668546))
* **credentials:** answer git and ssh prompts with gitnaga --askpass ([508f7c6](https://github.com/blackopsrepl/gitnaga/commit/508f7c6927240d1552471f4d4d3ab4643ba5ea31))
* **diff:** classify binary, image, submodule and LFS file diffs ([f2f7cf5](https://github.com/blackopsrepl/gitnaga/commit/f2f7cf5281b9cbb3c267fc71ae203780872fecb1))
* **diff:** pair, word-diff and highlight diff rows in C++ ([4aded2c](https://github.com/blackopsrepl/gitnaga/commit/4aded2cb396d98f82888ea99719cad48210949a9))
* **graph:** drag a branch chip onto a commit or branch to act on it ([317b36c](https://github.com/blackopsrepl/gitnaga/commit/317b36c60806ef537645090595ee4e820f1928dc))
* **navigation:** answer search, file history and blame queries ([4d53e66](https://github.com/blackopsrepl/gitnaga/commit/4d53e6699373ee2b932b4cac9497cd6b61963566))
* **operations:** drive half-done operations and record undo ([953f883](https://github.com/blackopsrepl/gitnaga/commit/953f883a14bdaa2d79e0c01afdb235038eecf521))
* **rebase:** add the interactive rebase editor and commit-menu quick actions ([45f4bc8](https://github.com/blackopsrepl/gitnaga/commit/45f4bc814396a8757b562b2fbda6de63030967d2))
* **rebase:** build interactive rebase todos and run them without an editor ([6872c72](https://github.com/blackopsrepl/gitnaga/commit/6872c728fb641d15868ba488f6ad2395d568eac2))
* **rebase:** expose rebase, branch moves and show-all to the window ([3ea48ef](https://github.com/blackopsrepl/gitnaga/commit/3ea48ef7be43169d671930805ac76c3e5558d69e))
* **refs:** hide refs from the sidebar row and show them all back ([6bea854](https://github.com/blackopsrepl/gitnaga/commit/6bea854bad2d14b93b35fdc4cbffa17bca5c6913))
* **staging:** build line-level patches for git apply ([db2c22d](https://github.com/blackopsrepl/gitnaga/commit/db2c22d6a08e54222a95b59bdcaea84fb56019d0))
* **staging:** move files, hunks and lines between worktree and index ([d56eb55](https://github.com/blackopsrepl/gitnaga/commit/d56eb558e4e25c50166596bfec024eecce1546c7))
* **sync:** RemoteSync controller with auto-fetch and lease-guarded push ([3529d7f](https://github.com/blackopsrepl/gitnaga/commit/3529d7fe4e5adc424673bf5eeb187f093ce00fc5))
* **sync:** fetch a repository as soon as it opens ([a6338c9](https://github.com/blackopsrepl/gitnaga/commit/a6338c9eb6156a712c74029de43fd362cb37aee1))
* **sync:** fetch, pull, push and upstream tracking in the git layer ([a3f3737](https://github.com/blackopsrepl/gitnaga/commit/a3f37379afaf835d34d67fb23f2a83c4f021a5f7))
* **toolbar:** Fetch, Pull and Push with ahead/behind counts ([1eca068](https://github.com/blackopsrepl/gitnaga/commit/1eca06824b7c2fad7bcd3673c9eebca05fe6f197))
* **ui:** diff view with split mode, word and syntax highlights, and file cards ([7fc78f8](https://github.com/blackopsrepl/gitnaga/commit/7fc78f8e3f658cee6837f8ee86cab473414cf5b9))
* **ui:** open a file's diff in the middle of the window ([346f762](https://github.com/blackopsrepl/gitnaga/commit/346f762fdeb0c05b73f52ec753a8997eda496811))
* **ui:** operation banner, conflict view and undo button ([4bf34f3](https://github.com/blackopsrepl/gitnaga/commit/4bf34f33844bb31da4bd9cca429e42faaaf8b574))
* **ui:** review uncommitted work as unstaged and staged sections ([88b7044](https://github.com/blackopsrepl/gitnaga/commit/88b70445471e5ca7a2571a97ec20413ac9bae692))
* **ui:** search, file history, blame, command palette and graph keys ([4ac2028](https://github.com/blackopsrepl/gitnaga/commit/4ac202812864ceb4574fc7400cc55f6c9bb1b854))
* **undo:** undo pulls and upstream changes; a push ends the history ([bcbe6b6](https://github.com/blackopsrepl/gitnaga/commit/bcbe6b692d1a5068dc0f9420a981966df23aa70f))


### Bug Fixes

* **conflicts:** report what Edit and Tool did instead of firing and forgetting ([345f204](https://github.com/blackopsrepl/gitnaga/commit/345f204240978fe2962248ed23e5e7c4da54b83d))
* **diff:** don't remember Ignore whitespace across sessions ([b1150f9](https://github.com/blackopsrepl/gitnaga/commit/b1150f925c4294eb429ed41837aac31fe725f492))
* **diff:** read patch headers only before the first hunk ([3ce63d1](https://github.com/blackopsrepl/gitnaga/commit/3ce63d1e08b6d87c76e1bf789cf7b743c2c69783))
* **diff:** say '1 commit' on the submodule card ([a2a9067](https://github.com/blackopsrepl/gitnaga/commit/a2a90679da1c4016512afcd80a83c72013c82947))
* **diff:** show whitespace on lines whose only change is whitespace ([2465211](https://github.com/blackopsrepl/gitnaga/commit/2465211873043e3a020ae95c1ac8bcbd0e56b8e0))
* **git:** lead failure messages with the verdict, and name refused credentials ([964c79d](https://github.com/blackopsrepl/gitnaga/commit/964c79dfda115642be7d2feb6a1262c791feb719))
* **graph:** size the lane column to the rows on screen ([61bfe43](https://github.com/blackopsrepl/gitnaga/commit/61bfe430fe9b6a15e8e321a64601568b6d460bbb))
* **menu:** size menus to their widest item ([1b9476b](https://github.com/blackopsrepl/gitnaga/commit/1b9476bb8a721a9bf3311c1c3ff2a564138bc3f4))
* **operations:** name rebase sides by branch and guide an idle bisect ([f766f95](https://github.com/blackopsrepl/gitnaga/commit/f766f955da4d1479522d01274e61649ebfea31ca))
* **rebase:** close the editor once its rebase finishes ([1b7c446](https://github.com/blackopsrepl/gitnaga/commit/1b7c446440fe639600a63436fbccd70f81e455de))
* **refs:** name stashes by what was stashed ([1d990a9](https://github.com/blackopsrepl/gitnaga/commit/1d990a9cc7012e3ec738ea2da78fa65e3e61f4c5))
* **sync:** keep the tooltip off an open sync menu ([b2fda86](https://github.com/blackopsrepl/gitnaga/commit/b2fda866d9dfeac7bbd18b4b96c524fa139350ab))
* **ui:** keep file names and dates readable in the review ([3ee4098](https://github.com/blackopsrepl/gitnaga/commit/3ee409832928bb7f746293d5fe4e4700f62490eb))
* **ui:** open the centred diff only when a person picks a file ([ff7415e](https://github.com/blackopsrepl/gitnaga/commit/ff7415ee1e29434406aef244bf8df5d08d36c3dd))
* **undo:** stop fetches from clearing the undo history ([44feaf5](https://github.com/blackopsrepl/gitnaga/commit/44feaf5e753524d3c98715ce327489f07216f16a))
* **undo:** undo a whole cherry-pick or revert sequence ([9c2cdf5](https://github.com/blackopsrepl/gitnaga/commit/9c2cdf5c79c5c28059d4d61c0f326b299d128434))


### Performance

* **graph:** sweep ancestry only as far as the viewport ([2ca6496](https://github.com/blackopsrepl/gitnaga/commit/2ca64966beaaf9f6f74a6ecacc5151397d2f34e3))
* **history:** write a commit-graph file for large histories ([0c71d98](https://github.com/blackopsrepl/gitnaga/commit/0c71d982210bf5df83dba80941c2c26ecdd62e28))
* **model:** index commits by a 64-bit key so streaming never stalls a frame ([963cf9c](https://github.com/blackopsrepl/gitnaga/commit/963cf9c48d4d628f2bab01a538927eb430f1d4a7))
* **model:** let the uncommitted row come and go without renumbering ([a9ad1f3](https://github.com/blackopsrepl/gitnaga/commit/a9ad1f3b06e2eed004cb6da2e599bc81d2f3438e))
* **trace:** measure load milestones and GUI stalls behind GITNAGA_PERF ([9b175b2](https://github.com/blackopsrepl/gitnaga/commit/9b175b21ae2e5f9f0ae072f46fe6af407b234c1a))


### Code Refactoring

* **core:** read refs, status and history separately ([1189760](https://github.com/blackopsrepl/gitnaga/commit/1189760c03cc7a5418ccf85495188e3e4aa93acf))


### Documentation

* describe GitNaga as it ships, with current numbers and screenshots ([f868b29](https://github.com/blackopsrepl/gitnaga/commit/f868b2996fbb1f9538927573d69831396b6ef621))
* **roadmap:** record the audit and the plan to beat GitKraken ([eed7729](https://github.com/blackopsrepl/gitnaga/commit/eed7729d92feeb460d47ee68ef6919f8ea4c2973))
* **wireframe:** specify the uncommitted row, review, diff view and keys as built ([5c054c5](https://github.com/blackopsrepl/gitnaga/commit/5c054c58fc858ebd23790382803ab86ecf02f313))


### Build System

* **release:** release with csemver ([c152a1a](https://github.com/blackopsrepl/gitnaga/commit/c152a1aef545cf7d87e371f8d50d2d80c3c8ee4f))


### Tests

* **acceptance:** drive fetch, pull, push and upstream through the window ([8582b4c](https://github.com/blackopsrepl/gitnaga/commit/8582b4ca9f5e94df9beb3f8b7fa131a2e5410d30))
* **acceptance:** drive search, history, blame, palette and graph keys ([fb7ad18](https://github.com/blackopsrepl/gitnaga/commit/fb7ad181ae3d865481ec063fd623cd8ca219663c))
* **acceptance:** drive the real window through a fixture zoo ([25099da](https://github.com/blackopsrepl/gitnaga/commit/25099da9f4312135534e49b4a892b0475313cb67))
* **acceptance:** look at banners, the conflict view and undo ([93eb074](https://github.com/blackopsrepl/gitnaga/commit/93eb074381ca23de10d2a5f72ee58e6f0b5d628f))
* **acceptance:** look at code, image, binary and whitespace diffs, and stage in the centred diff ([5aa3ea3](https://github.com/blackopsrepl/gitnaga/commit/5aa3ea3a60685d48393d6f1b9c29bbe18271c6b8))
* **acceptance:** look at staging, hunks, lines, amend and stashes ([83d94ad](https://github.com/blackopsrepl/gitnaga/commit/83d94ad9cbd95dfaf61efba6ac75add3e2210f9e))
* **acceptance:** look at the commit-menu quick actions ([0f2dbed](https://github.com/blackopsrepl/gitnaga/commit/0f2dbed52458d863b845158a3fe99d264a453c40))
* **acceptance:** open files as a click does, and check graph zoom keys ([d03a9ae](https://github.com/blackopsrepl/gitnaga/commit/d03a9ae9f61eac4405471bee1ba37ee45a0e6a9d))
* **acceptance:** unset an upstream from the branch menu and undo it from the footer ([7899235](https://github.com/blackopsrepl/gitnaga/commit/789923583a801f9fd001f45fa7e9aeacb2add574))
* **driver:** print lists and maps as JSON ([3e36fd0](https://github.com/blackopsrepl/gitnaga/commit/3e36fd00b99705db333941c81376e772390cae2f))
* **sync:** fetch through real authenticating http and ssh remotes ([a017cdc](https://github.com/blackopsrepl/gitnaga/commit/a017cdc2aaa9d2bd398474f9695b0d297eacb5b5))


## [0.1.22](https://github.com/blackopsrepl/gitnaga/compare/v0.1.21...v0.1.22) (2026-10-02)

### Bug Fixes

* **recents:** widen the fuzzy-match cursor to qsizetype ([fe103e5](https://github.com/blackopsrepl/gitnaga/commit/fe103e5321b73d9ddc110e211efaf9c28ae115c7))

## [0.1.21](https://github.com/blackopsrepl/gitnaga/compare/v0.1.20...v0.1.21) (2026-10-02)

### Features

* **remote:** delete a remote branch over the remote's own transport ([626d190](https://github.com/blackopsrepl/gitnaga/commit/626d190e32706491954ddbb4a66676ce2f660a25))

### Documentation

* **remote:** the delete works against any git server ([1b8cb48](https://github.com/blackopsrepl/gitnaga/commit/1b8cb4821b503a84231bea1b9f50997344aaa858))

### Tests

* **remote:** prove the delete against real git servers ([b3034c4](https://github.com/blackopsrepl/gitnaga/commit/b3034c4265f57a9a06f674202ee055f37fbaf898))

## [0.1.20](https://github.com/blackopsrepl/gitnaga/compare/v0.1.19...v0.1.20) (2026-10-02)

### Features

* **refs:** name the remote and branch inside a remote-tracking ref ([2ecd029](https://github.com/blackopsrepl/gitnaga/commit/2ecd029a55c1a9754880855ffb141eb3edcb7df9))
* **remote:** delete a branch on a GitHub remote through gh ([0a4bb8e](https://github.com/blackopsrepl/gitnaga/commit/0a4bb8e54994b33044c9ab335f7d2ba406a98e7a))
* **ui:** delete a remote branch from the references and the graph ([ce639cf](https://github.com/blackopsrepl/gitnaga/commit/ce639cf98ea1b186215065d5f30b9a1f77e6d4dd))

### Code Refactoring

* **controller:** share one off-thread operation runner ([b1f14e9](https://github.com/blackopsrepl/gitnaga/commit/b1f14e9c3f2cd2c87a21be5bc1bcbb757f056916))

### Documentation

* **branches:** describe deleting a remote branch ([2c8f508](https://github.com/blackopsrepl/gitnaga/commit/2c8f5087360938646782048eccdbfe17d7f2ab59))

### Tests

* **remote:** prove the remote delete against a stub gh ([9bd0fb7](https://github.com/blackopsrepl/gitnaga/commit/9bd0fb7cf875ab18da7075aa74e99554d836f1a2))

## [0.1.19](https://github.com/blackopsrepl/gitnaga/compare/v0.1.18...v0.1.19) (2026-10-02)

### Bug Fixes

* **build:** count source lines consistently ([08c8826](https://github.com/blackopsrepl/gitnaga/commit/08c882617e6a899175bc952cfc3ebefc98afca32))

## [0.1.18](https://github.com/blackopsrepl/gitnaga/compare/v0.1.17...v0.1.18) (2026-10-02)

### Features

* **ui:** add a linked worktree manager ([c93de46](https://github.com/blackopsrepl/gitnaga/commit/c93de46d513c181cb7755c6238a3b70138d3a9d9))
* **worktrees:** expose linked worktree lifecycle ([369a219](https://github.com/blackopsrepl/gitnaga/commit/369a2193906efa26fc4b7ea70b661b0abd55e1d2))

### Documentation

* **worktrees:** describe worktree management ([374d3bd](https://github.com/blackopsrepl/gitnaga/commit/374d3bd669c9532ee2cc6e34627e4e29e0504c96))

## [0.1.17](https://github.com/blackopsrepl/gitnaga/compare/v0.1.16...v0.1.17) (2026-09-22)

### Tests

* **wip:** split the work-in-progress tests into their own binary ([8c0026f](https://github.com/blackopsrepl/gitnaga/commit/8c0026f98999633d07fe727be4b2c075f48a3059))

## [0.1.16](https://github.com/blackopsrepl/gitnaga/compare/v0.1.15...v0.1.16) (2026-09-22)

### Features

* **wip): commit ticked files; feat(ui:** remembered projects and fuzzy switcher ([ec3e1fb](https://github.com/blackopsrepl/gitnaga/commit/ec3e1fb24e7f76bdeadd14d512996dd0b7bbf69b))

### Bug Fixes

* **graph:** let the uncommitted row continue its line's colour ([199de28](https://github.com/blackopsrepl/gitnaga/commit/199de28d862a1d2d2f20ef58d7f673d48aac84c7))
* **menu:** open a real popup on the uncommitted row and at the pointer ([732214c](https://github.com/blackopsrepl/gitnaga/commit/732214c0503c90b0ffba61739a7e56abc24d91a2))

## [0.1.15](https://github.com/blackopsrepl/gitnaga/compare/v0.1.14...v0.1.15) (2026-09-21)

### Bug Fixes

* **graph:** dim the traced line, not the row, and ramp the trace ([87fcdde](https://github.com/blackopsrepl/gitnaga/commit/87fcdde0f448f223dcb523052c9c609e4ed398fe))

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
