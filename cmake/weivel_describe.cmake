# weivel_describe(<target> HEADERS <header>... PREFIX <identifier prefix>
#                 SERVICE <definition struct> SYMBOL <description symbol> [ANSWERED_BY <function>]
#                 [SERVICE <struct> SYMBOL <symbol> [ANSWERED_BY <function>]]...
#                 [EXPORT <macro>] [STATUS <type>] [REFUSAL <type>] [VALUE <type>] [TREE <type>]
#                 [DESCRIBED_ELSEWHERE <definition struct>...])
#
# Runs weivel-describe at build time over HEADERS, in the order given, once per SERVICE, and adds each generated source,
# which defines SYMBOL answering the description of that definition, to <target>. ANSWERED_BY names the library function
# answering that definition, the symbol the service declares as its definition, which the description names as its
# `answered_by`; the source declares it, with EXPORT, unless a header declares it, and a service naming none leaves the
# field NULL, as a description only ever nested does. A function answers one definition, so no two services name the
# same one. PREFIX begins every other name a generated source declares. EXPORT is the macro SYMBOL is declared with,
# such as the library's export macro.
# STATUS, REFUSAL, VALUE and TREE name the types the headers use in those roles, defaulting to weivel/standard's own.
# The value and tree types are known by name alone, so the header declaring them is not one of HEADERS, which would
# refuse it; the headers that use them include it.
# DESCRIBED_ELSEWHERE names definitions another library describes, known by name alone in the same way: a pointer to
# one is a DEFINITION whose description is that library's `<definition struct>_description`, which <target> links.
# A definition HEADERS also declare is still described elsewhere. The source calls each such description once through
# pthread_once off Windows, so on glibc older than 2.34 <target> also links Threads::Threads (find_package(Threads)).
# A relative header is read from the calling directory. The generated sources include weivel/description.h and each
# header by its absolute path, so <target> is given weivel/standard's include directories.
# It is called from the directory that defines <target>: a generated source is known as generated only to its own
# directory under the policies of the standard's minimum CMake (CMP0118 OLD), so a call from elsewhere is refused.
# weivel-describe runs on the build machine, so a cross-compiled library cannot be described.

include_guard(GLOBAL)

function(weivel_describe target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "weivel_describe: ${target} is not a target")
    endif()
    if(NOT TARGET weivel-describe OR NOT TARGET weivel_standard)
        message(FATAL_ERROR "weivel_describe: add weivel/standard first, which builds weivel-describe")
    endif()
    get_target_property(defined_in ${target} SOURCE_DIR)
    if(NOT defined_in STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        message(FATAL_ERROR "weivel_describe: ${target} is defined in ${defined_in}, so call weivel_describe there; "
            "a source generated in ${CMAKE_CURRENT_SOURCE_DIR} would not be known as generated to ${target}")
    endif()
    set(single_keywords PREFIX EXPORT STATUS REFUSAL VALUE TREE)
    set(headers)
    set(elsewhere)
    set(services)
    set(symbols)
    set(answered)
    set(keyword)
    foreach(argument IN LISTS ARGN)
        if(argument STREQUAL "HEADERS" OR argument STREQUAL "SERVICE" OR argument STREQUAL "DESCRIBED_ELSEWHERE"
                OR argument IN_LIST single_keywords)
            if(keyword STREQUAL "SERVICE_NAMED")
                message(FATAL_ERROR "weivel_describe: SERVICE ${service} has no SYMBOL")
            elseif(keyword AND NOT keyword STREQUAL "HEADERS" AND NOT keyword STREQUAL "DESCRIBED_ELSEWHERE"
                    AND NOT keyword STREQUAL "SYMBOL_GIVEN" AND NOT keyword STREQUAL "ANSWERED_GIVEN")
                message(FATAL_ERROR "weivel_describe: ${keyword} has no value")
            endif()
            if(keyword STREQUAL "HEADERS" AND NOT headers)
                message(FATAL_ERROR "weivel_describe: HEADERS names no header")
            endif()
            if(keyword STREQUAL "DESCRIBED_ELSEWHERE" AND NOT elsewhere)
                message(FATAL_ERROR "weivel_describe: DESCRIBED_ELSEWHERE names no definition")
            endif()
            if(DEFINED describe_${argument} AND NOT argument STREQUAL "SERVICE")
                message(FATAL_ERROR "weivel_describe: ${argument} is given twice")
            endif()
            set(keyword ${argument})
        elseif(argument STREQUAL "SYMBOL")
            if(NOT keyword STREQUAL "SERVICE_NAMED")
                message(FATAL_ERROR "weivel_describe: SYMBOL must follow SERVICE <definition struct>")
            endif()
            set(keyword SYMBOL)
        elseif(argument STREQUAL "ANSWERED_BY")
            if(keyword STREQUAL "ANSWERED_GIVEN")
                message(FATAL_ERROR "weivel_describe: ANSWERED_BY is given twice for SERVICE ${service}")
            elseif(NOT keyword STREQUAL "SYMBOL_GIVEN")
                message(FATAL_ERROR
                    "weivel_describe: ANSWERED_BY must follow SERVICE <definition struct> SYMBOL <symbol>")
            endif()
            set(keyword ANSWERED_BY)
        elseif(keyword STREQUAL "ANSWERED_BY")
            if(argument IN_LIST answered)
                message(FATAL_ERROR "weivel_describe: ANSWERED_BY ${argument} is given for two services; a function "
                    "answers one definition, which one description describes")
            endif()
            list(LENGTH answered answered_count)
            math(EXPR last_answered "${answered_count} - 1")
            list(REMOVE_AT answered ${last_answered})
            list(APPEND answered ${argument})
            set(keyword ANSWERED_GIVEN)
        elseif(keyword STREQUAL "HEADERS")
            get_filename_component(header "${argument}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
            list(APPEND headers "${header}")
            set(describe_HEADERS 1)
        elseif(keyword STREQUAL "DESCRIBED_ELSEWHERE")
            list(APPEND elsewhere --described-elsewhere ${argument})
            set(describe_DESCRIBED_ELSEWHERE 1)
        elseif(keyword STREQUAL "SERVICE")
            set(service ${argument})
            list(APPEND services ${argument})
            set(keyword SERVICE_NAMED)
        elseif(keyword STREQUAL "SYMBOL")
            if(argument IN_LIST symbols)
                message(FATAL_ERROR "weivel_describe: SYMBOL ${argument} is given twice")
            endif()
            list(APPEND symbols ${argument})
            list(APPEND answered -)
            set(keyword SYMBOL_GIVEN)
        elseif(keyword AND keyword IN_LIST single_keywords)
            set(describe_${keyword} ${argument})
            set(keyword)
        else()
            message(FATAL_ERROR "weivel_describe: `${argument}` belongs to no keyword")
        endif()
    endforeach()
    if(keyword STREQUAL "SERVICE_NAMED")
        message(FATAL_ERROR "weivel_describe: SERVICE ${service} has no SYMBOL")
    elseif(keyword STREQUAL "DESCRIBED_ELSEWHERE" AND NOT elsewhere)
        message(FATAL_ERROR "weivel_describe: DESCRIBED_ELSEWHERE names no definition")
    elseif(keyword AND NOT keyword STREQUAL "HEADERS" AND NOT keyword STREQUAL "DESCRIBED_ELSEWHERE"
            AND NOT keyword STREQUAL "SYMBOL_GIVEN" AND NOT keyword STREQUAL "ANSWERED_GIVEN")
        message(FATAL_ERROR "weivel_describe: ${keyword} has no value")
    endif()
    if(NOT headers)
        message(FATAL_ERROR "weivel_describe: HEADERS names no header")
    endif()
    if(NOT DEFINED describe_PREFIX)
        message(FATAL_ERROR "weivel_describe: PREFIX is required")
    endif()
    if(NOT services)
        message(FATAL_ERROR "weivel_describe: name at least one SERVICE <definition struct> SYMBOL <symbol>")
    endif()

    set(options --prefix ${describe_PREFIX} ${elsewhere})

    foreach(role EXPORT STATUS REFUSAL VALUE TREE)
        if(DEFINED describe_${role})
            string(TOLOWER ${role} flag)
            list(APPEND options --${flag} ${describe_${role}})
        endif()
    endforeach()

    set(directory ${CMAKE_CURRENT_BINARY_DIR}/weivel_describe/${target})
    list(LENGTH services service_count)
    math(EXPR last "${service_count} - 1")
    foreach(at RANGE ${last})
        list(GET services ${at} service)
        list(GET symbols ${at} symbol)
        list(GET answered ${at} function)
        set(answering)
        if(NOT function STREQUAL "-")
            set(answering --answered-by ${function})
        endif()
        set(output ${directory}/${symbol}.c)
        add_custom_command(OUTPUT ${output}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${directory}
            COMMAND weivel-describe --definition ${service} --symbol ${symbol} ${answering} ${options}
                --output ${output} ${headers}
            DEPENDS weivel-describe ${headers}
            COMMENT "Describing ${service} as ${symbol}"
            VERBATIM)
        target_sources(${target} PRIVATE ${output})
    endforeach()
    target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:weivel_standard,INTERFACE_INCLUDE_DIRECTORIES>)
endfunction()
