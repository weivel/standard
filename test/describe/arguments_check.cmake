# -DSCENARIO=<answered_twice|answered_for_two> -DSTANDARD=<weivel/standard> -DWORK=<scratch directory>
# -DGENERATOR=<generator>
#
# Configures test/describe/arguments, which calls weivel_describe with the arguments SCENARIO names and stand-ins for
# the targets it requires, and expects the configure to be refused with that scenario's message. answered_twice:
# ANSWERED_BY given twice for one SERVICE. answered_for_two: two services naming the same ANSWERED_BY function.

cmake_minimum_required(VERSION 3.15)

if(SCENARIO STREQUAL "answered_twice")
    set(expected "weivel_describe: ANSWERED_BY is given twice for SERVICE described_mirror")
elseif(SCENARIO STREQUAL "answered_for_two")
    set(expected "weivel_describe: ANSWERED_BY described_mirror_definition is given for two services")
else()
    message(FATAL_ERROR "unknown SCENARIO `${SCENARIO}`")
endif()
file(REMOVE_RECURSE ${WORK})
execute_process(COMMAND ${CMAKE_COMMAND} -S ${STANDARD}/test/describe/arguments -B ${WORK} -G ${GENERATOR}
        -DSTANDARD=${STANDARD} -DSCENARIO=${SCENARIO}
    RESULT_VARIABLE code OUTPUT_VARIABLE printed ERROR_VARIABLE errors)
if(code STREQUAL "0")
    message(FATAL_ERROR "the ${SCENARIO} call of weivel_describe was configured: ${printed}")
endif()
string(REPLACE "\n" " " errors "${errors}")
string(REGEX REPLACE " +" " " errors "${errors}")
string(FIND "${errors}" "${expected}" found)
if(found EQUAL -1)
    message(FATAL_ERROR "expected `${expected}`, got: ${errors}")
endif()
