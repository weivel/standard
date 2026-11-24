cmake_minimum_required(VERSION 3.15)
project(describe_arguments NONE)

include(${STANDARD}/cmake/weivel_describe.cmake)
add_custom_target(weivel-describe)
add_custom_target(weivel_standard)
add_custom_target(described)
if(SCENARIO STREQUAL "answered_twice")
    weivel_describe(described HEADERS described.h PREFIX described_
        SERVICE described_mirror SYMBOL described_mirror_description
            ANSWERED_BY described_mirror_definition ANSWERED_BY described_other_definition)
elseif(SCENARIO STREQUAL "answered_for_two")
    weivel_describe(described HEADERS described.h PREFIX described_
        SERVICE described_mirror SYMBOL described_mirror_description ANSWERED_BY described_mirror_definition
        SERVICE described_walker SYMBOL described_walker_description ANSWERED_BY described_mirror_definition)
else()
    message(FATAL_ERROR "no scenario ${SCENARIO}")
endif()
