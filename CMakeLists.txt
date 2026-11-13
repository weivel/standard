cmake_minimum_required(VERSION 3.15)
project(weivel_standard C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

function(standard_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

set(WEIVEL_STANDARD_SOURCES
    src/json.c)

function(standard_target target)
    target_include_directories(${target} PUBLIC include)
    standard_strict_warnings(${target})
endfunction()

add_library(weivel_standard STATIC ${WEIVEL_STANDARD_SOURCES})
standard_target(weivel_standard)
set_target_properties(weivel_standard PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(weivel_standard_shared SHARED ${WEIVEL_STANDARD_SOURCES})
standard_target(weivel_standard_shared)
target_compile_definitions(weivel_standard_shared
    PRIVATE WEIVEL_STANDARD_BUILDING_SHARED
    INTERFACE WEIVEL_STANDARD_USING_SHARED)
set_target_properties(weivel_standard_shared PROPERTIES
    OUTPUT_NAME weivel_standard
    C_VISIBILITY_PRESET hidden
    ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/shared)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(test)
endif()
