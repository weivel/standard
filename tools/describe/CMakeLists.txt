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
