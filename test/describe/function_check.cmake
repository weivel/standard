# -DSTANDARD=<weivel/standard> -DCORE=<core checkout> -DWORK=<scratch directory> -DGENERATOR=<generator>
# [-DPLATFORM=<generator platform>] [-DTOOLSET=<generator toolset>] [-DCOMPILER=<C compiler>]
#
# Configures test/describe/elsewhere, which calls weivel_describe from a directory below the one defining its target,
# and expects the configure to be refused with the message naming the directory to call it from.

cmake_minimum_required(VERSION 3.15)

file(REMOVE_RECURSE ${WORK})
set(arguments -S ${STANDARD}/test/describe/elsewhere -B ${WORK} -G ${GENERATOR} -DSTANDARD=${STANDARD}
    -DWEIVEL_CORE_SOURCE_DIR=${CORE})
if(PLATFORM)
    list(APPEND arguments -A ${PLATFORM})
endif()
if(TOOLSET)
    list(APPEND arguments -T ${TOOLSET})
endif()
if(COMPILER)
    list(APPEND arguments -DCMAKE_C_COMPILER=${COMPILER})
endif()
execute_process(COMMAND ${CMAKE_COMMAND} ${arguments}
    RESULT_VARIABLE code OUTPUT_VARIABLE printed ERROR_VARIABLE errors)
if(code STREQUAL "0")
    message(FATAL_ERROR "a call of weivel_describe from another directory was configured: ${printed}")
endif()
string(REPLACE "\n" " " errors "${errors}")
string(REGEX REPLACE " +" " " errors "${errors}")
string(FIND "${errors}" "weivel_describe: elsewhere is defined in ${STANDARD}/test/describe/elsewhere, so call" found)
if(found EQUAL -1)
    message(FATAL_ERROR "expected the refusal naming the defining directory, got: ${errors}")
endif()
