cmake_minimum_required(VERSION 3.31)

include(cmake/GitNagaVersion.cmake)
project(GitNaga VERSION ${GITNAGA_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.9.1 REQUIRED COMPONENTS Concurrent Core Gui Qml Quick QuickControls2 Test)
qt_standard_project_setup(REQUIRES 6.9.1)

add_library(gitnaga_core STATIC
    src/avatar_provider.cpp
    src/avatar_provider.hpp
    src/commit_model.cpp
    src/commit_model.hpp
    src/conflict_parser.cpp
    src/conflict_parser.hpp
    src/filesystem_operations.cpp
    src/diff_line_model.cpp
    src/diff_line_model.hpp
    src/file_change_model.cpp
    src/file_change_model.hpp
    src/git_client.cpp
    src/git_client.hpp
    src/git_refs.cpp
    src/git_refs.hpp
    src/git_worktrees.cpp
    src/git_history.cpp
    src/git_status.cpp
    src/git_inspect.cpp
    src/git_parse.cpp
    src/git_parse.hpp
    src/git_rebase.cpp
    src/git_rebase.hpp
    src/git_remote.cpp
    src/git_repository.cpp
    src/git_sync.cpp
    src/git_sync.hpp
    src/graph_geometry.cpp
    src/graph_geometry.hpp
    src/graph_palette.hpp
    src/operation_status.cpp
    src/operation_status.hpp
    src/rebase_todo.cpp
    src/rebase_todo.hpp
    src/recent_projects.cpp
    src/recent_projects.hpp
    src/repository_conflicts.cpp
    src/repository_controller.cpp
    src/repository_controller.hpp
    src/repository_worktrees.cpp
    src/repository_loading.cpp
    src/repository_operations.cpp
    src/repository_rebase.cpp
    src/repository_selection.cpp
    src/repository_references.cpp
    src/remote_sync.cpp
    src/remote_sync.hpp
    src/remote_sync_refs.cpp
    src/repository_undo.cpp
    src/undo_capture.cpp
    src/undo_stack.cpp
    src/undo_stack.hpp
)

target_include_directories(gitnaga_core PUBLIC src)
target_link_libraries(gitnaga_core PUBLIC Qt6::Concurrent Qt6::Core Qt6::Gui)
target_compile_definitions(gitnaga_core PUBLIC QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII)
target_compile_options(gitnaga_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion>
)

qt_add_executable(gitnaga
    src/askpass.cpp
    src/askpass.hpp
    src/commit_graph_input.cpp
    src/commit_graph_item.cpp
    src/commit_graph_item.hpp
    src/commit_graph_paint.cpp
    src/main.cpp
    src/ui_driver.cpp
    src/ui_driver.hpp
)

set_source_files_properties(qml/NagaTheme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(gitnaga
    URI GitNaga
    VERSION 1.0
    NO_IMPORT_SCAN
    QML_FILES
        qml/Askpass.qml
        qml/BranchDragLayer.qml
        qml/BranchDropMenu.qml
        qml/CommitLabels.qml
        qml/CommitMenu.qml
        qml/ConfirmDialog.qml
        qml/ConflictBlockView.qml
        qml/ConflictDialog.qml
        qml/ConflictFileRow.qml
        qml/EmptyState.qml
        qml/GraphPane.qml
        qml/GraphScrollBar.qml
        qml/InspectorPane.qml
        qml/Main.qml
        qml/NagaButton.qml
        qml/NagaMenuItem.qml
        qml/NagaMenuSeparator.qml
        qml/NagaMenu.qml
        qml/OpenRepositoryDialog.qml
        qml/OperationBanner.qml
        qml/AddWorktreeDialog.qml
        qml/WorktreeDialog.qml
        qml/NagaIconButton.qml
        qml/NagaIcon.qml
        qml/NagaTheme.qml
        qml/PromptDialog.qml
        qml/RebaseActionMenu.qml
        qml/RebaseEditor.qml
        qml/RebaseEditorBody.qml
        qml/RebaseStepRow.qml
        qml/ReferencesPane.qml
        qml/RewordDialog.qml
        qml/ReferenceMenu.qml
        qml/RefRow.qml
        qml/RemoteChoiceDialog.qml
        qml/SyncButton.qml
        qml/SyncToolbar.qml
        qml/UpstreamDialog.qml
        qml/UndoButton.qml
)

target_compile_definitions(gitnaga PRIVATE
    GITNAGA_VERSION="${PROJECT_VERSION}"
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
)

target_compile_options(gitnaga PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion>
)

target_link_libraries(gitnaga PRIVATE
    gitnaga_core
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

include(GNUInstallDirs)
install(TARGETS gitnaga
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES packaging/io.gitnaga.GitNaga.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)
install(FILES packaging/io.gitnaga.GitNaga.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
)

include(CTest)
if(BUILD_TESTING)
    add_test(
        NAME source_size_contract
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/cmake/CheckSourceSize.cmake
    )
    add_subdirectory(tests)
endif()
