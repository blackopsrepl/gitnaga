qt_add_executable(gitnaga_smoke_test
    smoke_test.cpp
)
target_link_libraries(gitnaga_smoke_test PRIVATE Qt6::Core Qt6::Test)
add_test(NAME gitnaga_smoke_test COMMAND gitnaga_smoke_test)

qt_add_executable(gitnaga_git_test
    git_client_test.cpp
)
target_link_libraries(gitnaga_git_test PRIVATE gitnaga_core Qt6::Core Qt6::Test)
add_test(NAME gitnaga_git_test COMMAND gitnaga_git_test)

qt_add_executable(gitnaga_refs_test
    git_refs_test.cpp
)
target_link_libraries(gitnaga_refs_test PRIVATE gitnaga_core Qt6::Core Qt6::Test)
add_test(NAME gitnaga_refs_test COMMAND gitnaga_refs_test)

qt_add_executable(gitnaga_remote_test
    git_remote_test.cpp
)
target_link_libraries(gitnaga_remote_test PRIVATE gitnaga_core Qt6::Core Qt6::Test)
add_test(NAME gitnaga_remote_test COMMAND gitnaga_remote_test)

qt_add_executable(gitnaga_graph_test
    graph_geometry_test.cpp
)
target_link_libraries(gitnaga_graph_test PRIVATE gitnaga_core Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME gitnaga_graph_test COMMAND gitnaga_graph_test)

qt_add_executable(gitnaga_palette_test
    palette_test.cpp
)
target_link_libraries(gitnaga_palette_test PRIVATE gitnaga_core Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME gitnaga_palette_test COMMAND gitnaga_palette_test)

qt_add_executable(gitnaga_wip_test
    wip_test.cpp
)
target_link_libraries(gitnaga_wip_test PRIVATE gitnaga_core Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME gitnaga_wip_test COMMAND gitnaga_wip_test)

qt_add_executable(gitnaga_worktree_test
    worktree_test.cpp
)
target_link_libraries(gitnaga_worktree_test PRIVATE gitnaga_core Qt6::Core Qt6::Test)
add_test(NAME gitnaga_worktree_test COMMAND gitnaga_worktree_test)

qt_add_executable(gitnaga_controller_test
    controller_test.cpp
)
target_link_libraries(gitnaga_controller_test PRIVATE gitnaga_core Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME gitnaga_controller_test COMMAND gitnaga_controller_test)

qt_add_executable(gitnaga_sync_test
    git_sync_test.cpp
)
target_link_libraries(gitnaga_sync_test PRIVATE gitnaga_core Qt6::Core Qt6::Test)
add_test(NAME gitnaga_sync_test COMMAND gitnaga_sync_test)

qt_add_executable(gitnaga_sync_controller_test
    sync_controller_test.cpp
)
target_link_libraries(gitnaga_sync_controller_test PRIVATE gitnaga_core Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME gitnaga_sync_controller_test COMMAND gitnaga_sync_controller_test)

add_test(NAME gitnaga_askpass_test COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/askpass_test.sh $<TARGET_FILE:gitnaga>)

qt_add_executable(gitnaga_conflict_parser_test
    conflict_parser_test.cpp
)
target_link_libraries(gitnaga_conflict_parser_test PRIVATE gitnaga_core Qt6::Core Qt6::Test)
add_test(NAME gitnaga_conflict_parser_test COMMAND gitnaga_conflict_parser_test)

qt_add_executable(gitnaga_operations_test
    operations_test.cpp
)
target_link_libraries(gitnaga_operations_test PRIVATE gitnaga_core Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME gitnaga_operations_test COMMAND gitnaga_operations_test)

qt_add_executable(gitnaga_rebase_todo_test
    rebase_todo_test.cpp
)
target_link_libraries(gitnaga_rebase_todo_test PRIVATE gitnaga_core Qt6::Core Qt6::Test)
add_test(NAME gitnaga_rebase_todo_test COMMAND gitnaga_rebase_todo_test)

qt_add_executable(gitnaga_rebase_test
    rebase_test.cpp
)
target_link_libraries(gitnaga_rebase_test PRIVATE gitnaga_core Qt6::Core Qt6::Test)
add_test(NAME gitnaga_rebase_test COMMAND gitnaga_rebase_test)

qt_add_executable(gitnaga_branch_operations_test
    branch_operations_test.cpp
)
target_link_libraries(gitnaga_branch_operations_test PRIVATE gitnaga_core Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME gitnaga_branch_operations_test COMMAND gitnaga_branch_operations_test)

qt_add_executable(gitnaga_emphasis_test
    graph_emphasis_test.cpp
)
target_link_libraries(gitnaga_emphasis_test PRIVATE gitnaga_core Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME gitnaga_emphasis_test COMMAND gitnaga_emphasis_test)

qt_add_executable(gitnaga_patch_test
    patch_builder_test.cpp
)
target_link_libraries(gitnaga_patch_test PRIVATE gitnaga_core Qt6::Core Qt6::Test)
add_test(NAME gitnaga_patch_test COMMAND gitnaga_patch_test)

qt_add_executable(gitnaga_staging_controller_test
    staging_controller_test.cpp
)
target_link_libraries(gitnaga_staging_controller_test PRIVATE gitnaga_core Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME gitnaga_staging_controller_test COMMAND gitnaga_staging_controller_test)

# Every test runs headless with no desktop theme plugin: the GTK theme tries
# to open a display and the run dies before a single test executes.
get_property(gitnaga_tests DIRECTORY PROPERTY TESTS)
set_tests_properties(${gitnaga_tests} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=")
