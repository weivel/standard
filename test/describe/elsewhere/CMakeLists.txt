cmake_minimum_required(VERSION 3.15)
project(describe_elsewhere C)

add_subdirectory(${STANDARD} standard EXCLUDE_FROM_ALL)
add_library(elsewhere STATIC ${STANDARD}/test/describe/golden/paths.c)
add_subdirectory(inside)
