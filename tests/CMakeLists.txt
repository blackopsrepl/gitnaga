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
