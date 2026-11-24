# -DSCENARIO=<refused|usage|elsewhere|answered|unchanged> -DDESCRIBE=<weivel-describe> -DFIXTURES=<test/describe>
# -DWORK=<scratch directory>
#
# refused: a header outside the grammar exits 1, prints the refusal as `<file>:<line>: <message>` and leaves no
# source behind, even where an earlier run wrote one. usage: a call missing a required option exits 2 and writes
# nothing. elsewhere: `--described-elsewhere probe_remote` over nested.h, named from the fixtures directory as an
# include names it, writes golden/nested.c byte for byte. answered: `--answered-by probe_lender_definition` over
# nested_remote.h and nested.h writes golden/scanned.c byte for byte. unchanged: a second run over the same headers
# leaves the source
# it wrote untouched, its modification time included, so nothing built from it is rebuilt.

cmake_minimum_required(VERSION 3.15)

file(REMOVE_RECURSE ${WORK}/${SCENARIO})
file(MAKE_DIRECTORY ${WORK}/${SCENARIO})
set(output ${WORK}/${SCENARIO}/described.c)
file(WRITE ${output} "left by an earlier run\n")

if(SCENARIO STREQUAL "refused")
    set(header ${FIXTURES}/refused/union.h)
    execute_process(COMMAND ${DESCRIBE} --definition probe_service --symbol probe_described --prefix probe_
            --output ${output} ${header}
        RESULT_VARIABLE code OUTPUT_VARIABLE printed ERROR_VARIABLE errors)
    if(NOT code STREQUAL "1")
        message(FATAL_ERROR "expected exit 1 for a refused header, got ${code}: ${errors}")
    endif()
    string(FIND "${errors}" "${header}:3: a union is not in weivel's description grammar" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "expected the refusal at ${header}:3, got: ${errors}")
    endif()
    if(EXISTS ${output})
        message(FATAL_ERROR "a refused header left ${output} behind")
    endif()
elseif(SCENARIO STREQUAL "usage")
    execute_process(COMMAND ${DESCRIBE} --symbol probe_described --prefix probe_ --output ${output}
            ${FIXTURES}/every_form.h
        RESULT_VARIABLE code OUTPUT_VARIABLE printed ERROR_VARIABLE errors)
    if(NOT code STREQUAL "2")
        message(FATAL_ERROR "expected exit 2 for a call without --definition, got ${code}: ${errors}")
    endif()
    string(FIND "${errors}" "--definition, --symbol, --prefix and --output are required" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "expected the usage, got: ${errors}")
    endif()
    file(READ ${output} kept)
    if(NOT kept STREQUAL "left by an earlier run\n")
        message(FATAL_ERROR "a call refused for its usage changed ${output}")
    endif()
elseif(SCENARIO STREQUAL "elsewhere")
    execute_process(COMMAND ${DESCRIBE} --definition probe_lender --symbol nested_description_of --prefix nested_
            --described-elsewhere probe_remote --output ${output} nested.h
        WORKING_DIRECTORY ${FIXTURES}
        RESULT_VARIABLE code OUTPUT_VARIABLE printed ERROR_VARIABLE errors)
    if(NOT code STREQUAL "0")
        message(FATAL_ERROR "expected exit 0 describing nested.h, got ${code}: ${errors}")
    endif()
    file(READ ${output} written)
    file(READ ${FIXTURES}/golden/nested.c golden)
    if(NOT written STREQUAL golden)
        message(FATAL_ERROR "${output} differs from golden/nested.c")
    endif()
elseif(SCENARIO STREQUAL "answered")
    execute_process(COMMAND ${DESCRIBE} --definition probe_lender --symbol scanned_description_of --prefix scanned_
            --answered-by probe_lender_definition --described-elsewhere probe_remote --output ${output}
            nested_remote.h nested.h
        WORKING_DIRECTORY ${FIXTURES}
        RESULT_VARIABLE code OUTPUT_VARIABLE printed ERROR_VARIABLE errors)
    if(NOT code STREQUAL "0")
        message(FATAL_ERROR "expected exit 0 describing nested.h with its answering function, got ${code}: ${errors}")
    endif()
    file(READ ${output} written)
    file(READ ${FIXTURES}/golden/scanned.c golden)
    if(NOT written STREQUAL golden)
        message(FATAL_ERROR "${output} differs from golden/scanned.c")
    endif()
elseif(SCENARIO STREQUAL "unchanged")
    set(describe_nested ${DESCRIBE} --definition probe_lender --symbol nested_description_of --prefix nested_
        --described-elsewhere probe_remote --output ${output} nested.h)
    execute_process(COMMAND ${describe_nested} WORKING_DIRECTORY ${FIXTURES} RESULT_VARIABLE code ERROR_VARIABLE errors)
    if(NOT code STREQUAL "0")
        message(FATAL_ERROR "expected exit 0 describing nested.h, got ${code}: ${errors}")
    endif()
    file(TIMESTAMP ${output} first "%s" UTC)
    execute_process(COMMAND ${CMAKE_COMMAND} -E sleep 1.5)
    execute_process(COMMAND ${describe_nested} WORKING_DIRECTORY ${FIXTURES} RESULT_VARIABLE code ERROR_VARIABLE errors)
    if(NOT code STREQUAL "0")
        message(FATAL_ERROR "expected exit 0 describing nested.h again, got ${code}: ${errors}")
    endif()
    file(TIMESTAMP ${output} second "%s" UTC)
    if(NOT first STREQUAL second)
        message(FATAL_ERROR "describing unchanged headers rewrote ${output}: modified at ${first}, then at ${second}")
    endif()
else()
    message(FATAL_ERROR "no scenario ${SCENARIO}")
endif()
