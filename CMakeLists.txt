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

set(WEIVEL_CORE_SOURCE_DIR ${PROJECT_SOURCE_DIR}/siblings/core CACHE PATH
    "The core checkout the standard builds against when no weivel_core target exists yet")

if(NOT TARGET weivel_core)
    if(NOT EXISTS ${WEIVEL_CORE_SOURCE_DIR}/CMakeLists.txt)
        message(FATAL_ERROR "no core at ${WEIVEL_CORE_SOURCE_DIR}: link or clone the core checkout to siblings/core "
            "(for example a directory junction or symlink pointing at the core repository), "
            "or pass -DWEIVEL_CORE_SOURCE_DIR=<core checkout>")
    endif()
    add_subdirectory(${WEIVEL_CORE_SOURCE_DIR} core EXCLUDE_FROM_ALL)
endif()

set(WEIVEL_STANDARD_SOURCES
    src/allocation.c
    src/c_locale.c
    src/file_names.c
    src/identity.c
    src/json.c
    src/layout.c
    src/library.c
    src/manifest.c
    src/mirrors.c
    src/patterns.c
    src/report.c
    src/services.c
    src/sha256.c
    src/sized.c
    src/validate.c)
if(WIN32)
    list(APPEND WEIVEL_STANDARD_SOURCES src/files_windows.c)
else()
    list(APPEND WEIVEL_STANDARD_SOURCES src/files_posix.c)
endif()

function(standard_target target)
    target_include_directories(${target} PUBLIC include
        $<TARGET_PROPERTY:weivel_core,INTERFACE_INCLUDE_DIRECTORIES>)
    standard_strict_warnings(${target})
endfunction()

add_library(weivel_standard STATIC ${WEIVEL_STANDARD_SOURCES})
standard_target(weivel_standard)
set_target_properties(weivel_standard PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(weivel_standard_testing STATIC EXCLUDE_FROM_ALL ${WEIVEL_STANDARD_SOURCES})
standard_target(weivel_standard_testing)
target_include_directories(weivel_standard_testing PUBLIC src)
target_compile_definitions(weivel_standard_testing PUBLIC WEIVEL_STANDARD_TESTING)

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
