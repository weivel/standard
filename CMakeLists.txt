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
    src/description.c
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

add_library(weivel_standard_objects OBJECT ${WEIVEL_STANDARD_SOURCES})
standard_target(weivel_standard_objects)
set_target_properties(weivel_standard_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)

# weivel-describe links this one, since weivel_standard holds the descriptions weivel-describe generates.
add_library(weivel_standard_undescribed STATIC EXCLUDE_FROM_ALL $<TARGET_OBJECTS:weivel_standard_objects>)
standard_target(weivel_standard_undescribed)

add_library(weivel_standard STATIC $<TARGET_OBJECTS:weivel_standard_objects>)
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

add_subdirectory(tools/describe)
include(cmake/weivel_describe.cmake)

function(standard_descriptions target)
    set(headers ${PROJECT_SOURCE_DIR}/include/weivel)
    weivel_describe(${target}
        HEADERS ${headers}/services.h ${headers}/descriptions.h
        PREFIX standard_services_
        EXPORT WEIVEL_STANDARD_API
        SERVICE weivel_standard_config SYMBOL weivel_standard_config_description
        SERVICE weivel_standard_secrets SYMBOL weivel_standard_secrets_description
        SERVICE weivel_standard_paths SYMBOL weivel_standard_paths_description
        SERVICE weivel_standard_log SYMBOL weivel_standard_log_description)
    weivel_describe(${target}
        HEADERS ${headers}/services.h ${headers}/scope.h ${headers}/descriptions.h
        PREFIX standard_scope_
        EXPORT WEIVEL_STANDARD_API
        SERVICE weivel_standard_scopes SYMBOL weivel_standard_scopes_description
        DESCRIBED_ELSEWHERE weivel_standard_config weivel_standard_secrets weivel_standard_paths weivel_standard_log)
    weivel_describe(${target}
        HEADERS ${headers}/services.h ${headers}/host.h ${headers}/descriptions.h
        PREFIX standard_host_
        EXPORT WEIVEL_STANDARD_API
        SERVICE weivel_standard_host SYMBOL weivel_standard_host_description)
    if(NOT WIN32)
        target_link_libraries(${target} PUBLIC ${CMAKE_THREAD_LIBS_INIT})
    endif()
endfunction()

if(NOT WIN32)
    find_package(Threads REQUIRED)
endif()
foreach(described weivel_standard weivel_standard_testing weivel_standard_shared)
    standard_descriptions(${described})
endforeach()

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(test)
endif()
