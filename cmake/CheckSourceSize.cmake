file(GLOB_RECURSE sources
    "${SOURCE_ROOT}/src/*.cpp"
    "${SOURCE_ROOT}/src/*.hpp"
    "${SOURCE_ROOT}/qml/*.qml"
    "${SOURCE_ROOT}/tests/*.cpp"
    "${SOURCE_ROOT}/tests/*.hpp"
)

foreach(source IN LISTS sources)
    file(STRINGS "${source}" lines)
    list(LENGTH lines line_count)
    if(line_count GREATER_EQUAL 300)
        file(RELATIVE_PATH relative "${SOURCE_ROOT}" "${source}")
        message(FATAL_ERROR "${relative} has ${line_count} lines; source files must stay below 300")
    endif()
endforeach()
