# GitNaga Makefile v0.1.0
# Qt 6 / CMake / Ninja build system with colorized output

# ============== Colors & Symbols ==============
GREEN := \033[92m
EMERALD := \033[38;2;16;185;129m
CYAN := \033[96m
YELLOW := \033[93m
MAGENTA := \033[95m
RED := \033[91m
GRAY := \033[90m
BOLD := \033[1m
RESET := \033[0m

CHECK := ✓
CROSS := ✗
ARROW := ▸
PROGRESS := →

# ============== Project Metadata ==============
VERSION := $(shell sed -n 's/^set(GITNAGA_VERSION "\(.*\)")/\1/p' cmake/GitNagaVersion.cmake)
QT_MIN := 6.9.1
QT_QML_DIR ?= $(shell qtpaths6 --query QT_INSTALL_QML 2>/dev/null || echo /usr/lib64/qt6/qml)
QT_BIN_DIR ?= $(shell qtpaths6 --query QT_INSTALL_BINS 2>/dev/null)
QML_LINT ?= $(if $(QT_BIN_DIR),$(QT_BIN_DIR)/qmllint,qmllint)
REPO ?= $(CURDIR)

# ============== Phony Targets ==============
.PHONY: banner help configure build build-release run test lint ci-local pre-release \
        release-check release-dry release release-push release-verify \
        version bump-patch bump-minor bump-major bump-dry clean watch

# ============== Default Target ==============
.DEFAULT_GOAL := help

# ============== Banner ==============
banner:
	@printf "$(EMERALD)$(BOLD)"
	@printf " ██████╗ ██╗████████╗███╗   ██╗ █████╗  ██████╗  █████╗ \n"
	@printf "██╔════╝ ██║╚══██╔══╝████╗  ██║██╔══██╗██╔════╝ ██╔══██╗\n"
	@printf "██║  ███╗██║   ██║   ██╔██╗ ██║███████║██║  ███╗███████║\n"
	@printf "██║   ██║██║   ██║   ██║╚██╗██║██╔══██║██║   ██║██╔══██║\n"
	@printf "╚██████╔╝██║   ██║   ██║ ╚████║██║  ██║╚██████╔╝██║  ██║\n"
	@printf " ╚═════╝ ╚═╝   ╚═╝   ╚═╝  ╚═══╝╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═╝\n"
	@printf "$(RESET)"
	@printf "  $(GRAY)v$(VERSION)$(RESET) $(EMERALD)native Git history and diffs$(RESET)\n\n"

# ============== Configure & Build ==============

configure:
	@printf "$(PROGRESS) Configuring development build...\n"
	@cmake --preset dev >/dev/null && \
		printf "$(GREEN)$(CHECK) Configured$(RESET)\n" || \
		(printf "$(RED)$(CROSS) Configure failed$(RESET)\n" && exit 1)

build: banner
	@printf "$(CYAN)$(BOLD)╔══════════════════════════════════════╗$(RESET)\n"
	@printf "$(CYAN)$(BOLD)║          Building GitNaga            ║$(RESET)\n"
	@printf "$(CYAN)$(BOLD)╚══════════════════════════════════════╝$(RESET)\n\n"
	@printf "$(ARROW) $(BOLD)Debug build...$(RESET)\n"
	@cmake --preset dev >/dev/null
	@cmake --build --preset dev && \
		printf "$(GREEN)$(CHECK) Build successful$(RESET)\n\n" || \
		(printf "$(RED)$(CROSS) Build failed$(RESET)\n\n" && exit 1)

build-release: banner
	@printf "$(CYAN)$(BOLD)╔══════════════════════════════════════╗$(RESET)\n"
	@printf "$(CYAN)$(BOLD)║        Release Build                 ║$(RESET)\n"
	@printf "$(CYAN)$(BOLD)╚══════════════════════════════════════╝$(RESET)\n\n"
	@printf "$(ARROW) $(BOLD)Optimized build...$(RESET)\n"
	@cmake --preset release >/dev/null
	@cmake --build --preset release && \
		printf "$(GREEN)$(CHECK) Release build successful$(RESET)\n\n" || \
		(printf "$(RED)$(CROSS) Release build failed$(RESET)\n\n" && exit 1)

run: build
	@printf "$(PROGRESS) Launching GitNaga on $(YELLOW)$(REPO)$(RESET)\n"
	@./build/dev/gitnaga $(REPO)

# ============== Test & Lint ==============

test: build
	@printf "$(CYAN)$(BOLD)╔══════════════════════════════════════╗$(RESET)\n"
	@printf "$(CYAN)$(BOLD)║         Full Test Suite              ║$(RESET)\n"
	@printf "$(CYAN)$(BOLD)╚══════════════════════════════════════╝$(RESET)\n\n"
	@printf "$(ARROW) $(BOLD)Running ctest...$(RESET)\n"
	@ctest --preset dev && \
		printf "\n$(GREEN)$(CHECK) All tests passed$(RESET)\n\n" || \
		(printf "\n$(RED)$(CROSS) Tests failed$(RESET)\n\n" && exit 1)

lint: build
	@printf "$(PROGRESS) Linting QML...\n"
	@$(QML_LINT) -W 0 -I build/dev -I "$(QT_QML_DIR)" qml/*.qml && \
		printf "$(GREEN)$(CHECK) QML clean$(RESET)\n" || \
		(printf "$(RED)$(CROSS) QML lint failed$(RESET)\n" && exit 1)

# ============== CI Simulation ==============

ci-local: banner
	@printf "$(CYAN)$(BOLD)╔══════════════════════════════════════════════════════════╗$(RESET)\n"
	@printf "$(CYAN)$(BOLD)║              Local CI Simulation                         ║$(RESET)\n"
	@printf "$(CYAN)$(BOLD)╚══════════════════════════════════════════════════════════╝$(RESET)\n\n"
	@printf "$(PROGRESS) Step 1/4: Configure...\n"
	@cmake --preset dev >/dev/null && printf "$(GREEN)$(CHECK) Configured$(RESET)\n"
	@printf "$(PROGRESS) Step 2/4: Build...\n"
	@cmake --build --preset dev >/dev/null && printf "$(GREEN)$(CHECK) Build passed$(RESET)\n"
	@printf "$(PROGRESS) Step 3/4: Tests...\n"
	@ctest --preset dev >/dev/null && printf "$(GREEN)$(CHECK) Tests passed$(RESET)\n"
	@printf "$(PROGRESS) Step 4/4: QML lint...\n"
	@$(QML_LINT) -W 0 -I build/dev -I "$(QT_QML_DIR)" qml/*.qml >/dev/null && printf "$(GREEN)$(CHECK) QML clean$(RESET)\n"
	@printf "\n$(GREEN)$(BOLD)╔══════════════════════════════════════════════════════════╗$(RESET)\n"
	@printf "$(GREEN)$(BOLD)║              $(CHECK) CI SIMULATION PASSED                      ║$(RESET)\n"
	@printf "$(GREEN)$(BOLD)╚══════════════════════════════════════════════════════════╝$(RESET)\n\n"

pre-release: banner
	@printf "$(CYAN)$(BOLD)╔══════════════════════════════════════════════════════════╗$(RESET)\n"
	@printf "$(CYAN)$(BOLD)║            Pre-Release Validation v$(VERSION)                  ║$(RESET)\n"
	@printf "$(CYAN)$(BOLD)╚══════════════════════════════════════════════════════════╝$(RESET)\n\n"
	@$(MAKE) build --no-print-directory >/dev/null
	@printf "$(PROGRESS) Running full test suite...\n"
	@ctest --preset dev >/dev/null && printf "$(GREEN)$(CHECK) All tests passed$(RESET)\n"
	@printf "$(PROGRESS) Linting QML...\n"
	@$(QML_LINT) -W 0 -I build/dev -I "$(QT_QML_DIR)" qml/*.qml >/dev/null && printf "$(GREEN)$(CHECK) QML clean$(RESET)\n"
	@printf "\n$(GREEN)$(BOLD)$(CHECK) Ready for release v$(VERSION)$(RESET)\n\n"

# ============== Release (commit-and-tag-version) ==============
#
# The version surface and the AppStream release history are owned by
# commit-and-tag-version through .versionrc.js. Never hand-edit them.

release-check:
	@scripts/release-config-check.sh

release-dry: banner
	@printf "$(ARROW) Previewing the next release...\n\n"
	@npx commit-and-tag-version --dry-run

release: banner
	@$(MAKE) pre-release --no-print-directory
	@scripts/release-config-check.sh
	@printf "$(ARROW) Bumping the version, changelog, and tag...\n\n"
	@npx commit-and-tag-version
	@printf "\n$(GREEN)$(CHECK) Released $$(git describe --tags --abbrev=0)$(RESET)\n"
	@printf "$(GRAY)Push it with: make release-push$(RESET)\n\n"

release-push: banner
	@scripts/push-release.sh
	@printf "\n"
	@scripts/verify-release.sh

release-verify:
	@scripts/verify-release.sh

# ============== Version Management ==============

version:
	@printf "$(CYAN)Current version:$(RESET) $(YELLOW)$(BOLD)$(VERSION)$(RESET)\n"

# Manual bumps. These call the tool directly and bypass the pre-release gate;
# prefer `make release`.

bump-patch: banner
	@printf "$(ARROW) Bumping patch version...\n"
	@npx commit-and-tag-version --release-as patch
	@printf "$(GREEN)$(CHECK) Version bumped$(RESET)\n"

bump-minor: banner
	@printf "$(ARROW) Bumping minor version...\n"
	@npx commit-and-tag-version --release-as minor
	@printf "$(GREEN)$(CHECK) Version bumped$(RESET)\n"

bump-major: banner
	@printf "$(ARROW) Bumping major version...\n"
	@npx commit-and-tag-version --release-as major
	@printf "$(GREEN)$(CHECK) Version bumped$(RESET)\n"

bump-dry:
	@npx commit-and-tag-version --dry-run

# ============== Housekeeping ==============

clean:
	@printf "$(ARROW) Cleaning build artifacts...\n"
	@rm -rf build
	@printf "$(GREEN)$(CHECK) Clean complete$(RESET)\n"

watch:
	@printf "$(ARROW) Watching for changes...\n"
	@while true; do cmake --build --preset dev; sleep 1; done

# ============== Help ==============

help: banner
	@/bin/echo -e "$(CYAN)$(BOLD)Build Commands:$(RESET)"
	@/bin/echo -e "  $(GREEN)make configure$(RESET)      - Configure the debug preset"
	@/bin/echo -e "  $(GREEN)make build$(RESET)          - Build the debug preset"
	@/bin/echo -e "  $(GREEN)make build-release$(RESET)  - Build the release preset"
	@/bin/echo -e "  $(GREEN)make run REPO=path$(RESET)  - Build and run against a repository"
	@/bin/echo -e ""
	@/bin/echo -e "$(CYAN)$(BOLD)Test & Lint:$(RESET)"
	@/bin/echo -e "  $(GREEN)make test$(RESET)           - Build and run the ctest suite"
	@/bin/echo -e "  $(GREEN)make lint$(RESET)           - Build and lint every QML file"
	@/bin/echo -e ""
	@/bin/echo -e "$(CYAN)$(BOLD)Quality:$(RESET)"
	@/bin/echo -e "  $(GREEN)make ci-local$(RESET)       - $(YELLOW)$(BOLD)Simulate the CI workflow locally$(RESET)"
	@/bin/echo -e "  $(GREEN)make pre-release$(RESET)    - Run all validation checks"
	@/bin/echo -e ""
	@/bin/echo -e "$(CYAN)$(BOLD)Release:$(RESET)"
	@/bin/echo -e "  $(GREEN)make release-check$(RESET)  - Validate the release surfaces and tree"
	@/bin/echo -e "  $(GREEN)make release-dry$(RESET)    - Preview the next release"
	@/bin/echo -e "  $(GREEN)make release$(RESET)        - $(YELLOW)$(BOLD)Gate, then bump version, changelog, and tag$(RESET)"
	@/bin/echo -e "  $(GREEN)make release-push$(RESET)   - Push branch and tag, then verify both remotes"
	@/bin/echo -e "  $(GREEN)make release-verify$(RESET) - Verify the published release only"
	@/bin/echo -e ""
	@/bin/echo -e "$(CYAN)$(BOLD)Version Management:$(RESET)"
	@/bin/echo -e "  $(GREEN)make version$(RESET)        - Show current version"
	@/bin/echo -e "  $(GREEN)make bump-patch$(RESET)     - Manual patch bump (bypasses the gate)"
	@/bin/echo -e "  $(GREEN)make bump-minor$(RESET)     - Manual minor bump (bypasses the gate)"
	@/bin/echo -e "  $(GREEN)make bump-major$(RESET)     - Manual major bump (bypasses the gate)"
	@/bin/echo -e "  $(GREEN)make bump-dry$(RESET)       - Preview the version bump"
	@/bin/echo -e ""
	@/bin/echo -e "$(CYAN)$(BOLD)Other:$(RESET)"
	@/bin/echo -e "  $(GREEN)make clean$(RESET)          - Remove build artifacts"
	@/bin/echo -e "  $(GREEN)make watch$(RESET)          - Rebuild on change"
	@/bin/echo -e "  $(GREEN)make help$(RESET)           - Show this help message"
	@/bin/echo -e ""
	@/bin/echo -e "$(GRAY)Qt $(QT_MIN)+ required: $(RESET)"
	@/bin/echo -e "$(GRAY)Current version: v$(VERSION)$(RESET)"
	@/bin/echo -e ""
