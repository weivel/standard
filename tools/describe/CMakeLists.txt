set(WEIVEL_DESCRIBE_READER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/model.c
    ${CMAKE_CURRENT_SOURCE_DIR}/parse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/reader.c
    ${CMAKE_CURRENT_SOURCE_DIR}/resolve.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tokens.c)

function(describe_reader target standard)
    add_library(${target} STATIC ${ARGN} ${WEIVEL_DESCRIBE_READER_SOURCES})
    target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(${target} PUBLIC ${standard})
    standard_strict_warnings(${target})
endfunction()

describe_reader(weivel_describe_reader weivel_standard)
describe_reader(weivel_describe_reader_testing weivel_standard_testing EXCLUDE_FROM_ALL)

if(WIN32)
    set(WEIVEL_DESCRIBE_PLATFORM_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/platform_windows.c)
else()
    set(WEIVEL_DESCRIBE_PLATFORM_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/platform_posix.c)
endif()

function(describe_writer target reader)
    add_library(${target} STATIC ${ARGN} ${CMAKE_CURRENT_SOURCE_DIR}/writer.c ${WEIVEL_DESCRIBE_PLATFORM_SOURCE})
    target_link_libraries(${target} PUBLIC ${reader})
    if(WIN32)
        target_link_libraries(${target} PUBLIC shell32)
    endif()
    standard_strict_warnings(${target})
endfunction()

describe_writer(weivel_describe_writer weivel_describe_reader)
describe_writer(weivel_describe_writer_testing weivel_describe_reader_testing EXCLUDE_FROM_ALL)

add_executable(weivel-describe main.c)
target_link_libraries(weivel-describe PRIVATE weivel_describe_writer)
standard_strict_warnings(weivel-describe)
