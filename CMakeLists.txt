cmake_minimum_required(VERSION 3.31)

include(cmake/GitNagaVersion.cmake)
project(GitNaga VERSION ${GITNAGA_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.9.1 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Test)
qt_standard_project_setup(REQUIRES 6.9.1)

qt_add_executable(gitnaga
    src/main.cpp
)

qt_add_qml_module(gitnaga
    URI GitNaga
    VERSION 1.0
    NO_IMPORT_SCAN
    QML_FILES
        qml/Main.qml
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
    add_subdirectory(tests)
endif()
