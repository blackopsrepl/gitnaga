cmake_minimum_required(VERSION 3.31)

file(GLOB_RECURSE sources
    "${SOURCE_ROOT}/src/*.cpp"
    "${SOURCE_ROOT}/src/*.hpp"
    "${SOURCE_ROOT}/qml/*.qml"
    "${SOURCE_ROOT}/tests/*.cpp"
    "${SOURCE_ROOT}/tests/*.hpp"
)

foreach(source IN LISTS sources)
    file(READ "${source}" contents)
    string(REGEX MATCHALL "\n" line_breaks "${contents}")
    list(LENGTH line_breaks line_count)
    string(LENGTH "${contents}" content_length)
    if(content_length GREATER 0)
        math(EXPR last_index "${content_length} - 1")
        string(SUBSTRING "${contents}" ${last_index} 1 last_character)
        if(NOT last_character STREQUAL "\n")
            math(EXPR line_count "${line_count} + 1")
        endif()
    endif()
    if(line_count GREATER_EQUAL 300)
        file(RELATIVE_PATH relative "${SOURCE_ROOT}" "${source}")
        message(FATAL_ERROR "${relative} has ${line_count} lines; source files must stay below 300")
    endif()
endforeach()
