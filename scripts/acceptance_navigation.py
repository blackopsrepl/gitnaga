"""Acceptance scenarios for search, file history, blame, the command palette
and graph keyboard navigation. Imported by acceptance.py."""


def navigation_scenarios(zoo, shot, big_count):
    # Queries scale with the big fixture: its last commit writes its index.
    last = big_count - 1
    history = str(zoo / "history")
    big = str(zoo / "big")
    settle = {"until": "navigation.search.running", "equals": False}
    return {
        "search-text": [
            {"open": str(zoo / "branches")}, {"idle": True},
            {"key": "Ctrl+F"}, {"type": "search"}, {"wait": 400}, settle, {"idle": True},
            {"print": "navigation.search.count"}, {"print": "navigation.search.position"},
            {"key": "Return"}, {"idle": True}, shot("search-text"),
            {"print": "navigation.search.position"},
            {"expect": {"navigation.search.count": "2", "navigation.search.position": "2"}},
        ],
        "search-big": [
            {"open": big}, {"idle": True},
            {"key": "Ctrl+F"}, {"type": f"change {last // 2}"}, {"key": "Return"}, settle, {"idle": True},
            {"print": "navigation.search.count"}, {"print": "navigation.search.elapsedMs"},
            shot("search-big-text"),
            {"key": "Escape"}, {"key": "Ctrl+F"}, {"type": "path:f7.txt"}, {"key": "Return"}, settle, {"idle": True},
            {"print": "navigation.search.kind"}, {"print": "navigation.search.count"},
            {"print": "navigation.search.elapsedMs"}, shot("search-big-path"),
            {"key": "Escape"}, {"key": "Ctrl+F"}, {"type": f"content:{last}"}, {"key": "Return"}, settle, {"idle": True},
            {"print": "navigation.search.kind"}, {"print": "navigation.search.count"},
            {"print": "navigation.search.elapsedMs"},
            {"expect": {"navigation.search.kind": "content", "navigation.search.count": "1"}},
        ],
        "file-history": [
            {"open": history}, {"idle": True}, {"select": "WORKTREE"}, {"idle": True},
            {"rightClick": [980, 294]}, {"wait": 300}, shot("file-menu"),
            {"click": [1044, 312]}, {"until": "navigation.fileHistory.loading", "equals": False}, {"idle": True},
            {"print": "navigation.fileHistory.count"}, shot("file-history"),
            {"key": "Down"}, {"idle": True}, shot("file-history-rename"),
            {"expect": {"navigation.fileHistory.count": "4"}},
        ],
        "blame": [
            {"open": history}, {"idle": True}, {"select": "WORKTREE"}, {"idle": True}, {"file": 0}, {"idle": True},
            {"key": "Ctrl+Shift+B"}, {"until": "navigation.blame.loading", "equals": False}, {"idle": True},
            {"print": "navigation.blame.count"}, {"print": "navigation.blame.commitCount"}, shot("blame"),
            {"click": [880, 175]}, {"idle": True}, shot("blame-selected"), {"print": "selectedCommit.subject"},
            {"expect": {"navigation.blame.count": "8", "navigation.blame.commitCount": "4"}},
        ],
        "palette": [
            {"open": str(zoo / "branches")}, {"idle": True},
            {"key": "Ctrl+K"}, {"wait": 200}, shot("palette-all"),
            {"type": "feature/sea"}, {"wait": 200}, shot("palette-filtered"),
            {"key": "Return"}, {"wait": 300}, {"idle": True}, {"print": "currentBranch"},
            {"key": "Ctrl+Shift+P"}, {"type": "zoom in"}, {"key": "Return"}, {"wait": 200}, shot("palette-zoomed"),
            {"expect": {"currentBranch": "feature/search"}},
        ],
        "graph-keys": [
            {"open": history}, {"idle": True}, {"click": [640, 128]}, {"idle": True},
            {"key": "Down"}, {"key": "J"}, {"idle": True}, {"print": "selectedRow"},
            {"key": "Left"}, {"idle": True}, {"print": "selectedCommit.subject"},
            {"key": "Right"}, {"key": "Right"}, {"idle": True}, {"print": "selectedRow"},
            {"key": "End"}, {"idle": True}, {"print": "selectedCommit.subject"},
            {"key": "Home"}, {"key": "K"}, {"idle": True}, {"print": "selectedRow"},
            {"key": "PgDown"}, {"idle": True}, {"print": "selectedRow"},
            {"key": "Alt+Up"}, {"idle": True}, shot("graph-keys"), {"print": "selectedRow"},
            {"expect": {"selectedCommit.subject": "docs: start the notes"}},
        ],
    }
