function(standard_test name source)
    add_executable(${name} ${source}.c)
    target_link_libraries(${name} PRIVATE ${ARGN})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    standard_strict_warnings(${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 60)
endfunction()

standard_test(abi_test abi_test weivel_standard)
standard_test(abi_shared_test abi_test weivel_standard_shared)
standard_test(json_test json_test weivel_standard)
standard_test(json_shared_test json_test weivel_standard_shared)
standard_test(json_equality_test json_equality_test weivel_standard)
target_include_directories(json_equality_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
standard_test(manifest_test manifest_test weivel_standard weivel_core)
standard_test(manifest_shared_test manifest_test weivel_standard_shared weivel_core)
standard_test(manifest_memory_test manifest_memory_test weivel_standard_testing)
standard_test(json_memory_test json_memory_test weivel_standard_testing)
standard_test(validate_test validate_test weivel_standard)
target_sources(validate_test PRIVATE validate_rules.c)
standard_test(validate_shared_test validate_test weivel_standard_shared)
target_sources(validate_shared_test PRIVATE validate_rules.c)
standard_test(validate_memory_test validate_memory_test weivel_standard_testing)
standard_test(pattern_test pattern_test weivel_standard)
target_include_directories(pattern_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
standard_test(schema_parity_test schema_parity_test weivel_standard)
target_sources(schema_parity_test PRIVATE validate_rules.c)
target_include_directories(schema_parity_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(schema_parity_test PRIVATE
    WEIVEL_STANDARD_SCHEMA_PATH="${PROJECT_SOURCE_DIR}/schema/plugin.schema.json")
standard_test(identity_test identity_test weivel_standard)
target_include_directories(identity_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
standard_test(identity_memory_test identity_memory_test weivel_standard_testing)
standard_test(library_test library_test weivel_standard)
standard_test(library_shared_test library_test weivel_standard_shared)
standard_test(library_memory_test library_memory_test weivel_standard_testing)
standard_test(description_test description_test weivel_standard)
standard_test(description_shared_test description_test weivel_standard_shared)
standard_test(description_memory_test description_memory_test weivel_standard_testing)
foreach(description_target description_test description_shared_test description_memory_test)
    target_sources(${description_target} PRIVATE description_fixture.c)
endforeach()

function(layout_test name source)
    standard_test(${name} ${source} ${ARGN})
    target_sources(${name} PRIVATE fixtures.c)
    target_compile_definitions(${name} PRIVATE
        WEIVEL_STANDARD_FIXTURE_ROOT="${CMAKE_CURRENT_BINARY_DIR}/fixtures-${name}")
endfunction()
layout_test(layout_test layout_test weivel_standard)
layout_test(layout_shared_test layout_test weivel_standard_shared)
layout_test(layout_memory_test layout_memory_test weivel_standard_testing)

standard_test(services_test services_test weivel_standard)
standard_test(services_shared_test services_test weivel_standard_shared)
standard_test(sha256_test sha256_test weivel_standard)
standard_test(sha256_shared_test sha256_test weivel_standard_shared)
standard_test(mirrors_test mirrors_test weivel_standard)
standard_test(mirrors_shared_test mirrors_test weivel_standard_shared)
standard_test(mirrors_memory_test mirrors_memory_test weivel_standard_testing)
foreach(mirrors_target mirrors_test mirrors_shared_test)
    target_compile_definitions(${mirrors_target} PRIVATE
        WEIVEL_STANDARD_MIRRORS_FIXTURE="${CMAKE_CURRENT_BINARY_DIR}/${mirrors_target}-fixture.json")
endforeach()

add_executable(layouts layouts/layouts.c)
target_include_directories(layouts PRIVATE ${PROJECT_SOURCE_DIR}/include
    $<TARGET_PROPERTY:weivel_core,INTERFACE_INCLUDE_DIRECTORIES>)
standard_strict_warnings(layouts)
add_test(NAME layouts_list_every_public_struct_and_field
    COMMAND ${CMAKE_COMMAND} -DLAYOUTS=$<TARGET_FILE:layouts>
        -DHEADERS=${PROJECT_SOURCE_DIR}/include -P ${CMAKE_CURRENT_SOURCE_DIR}/layouts/layouts_check.cmake)
foreach(scenario
        layouts_check_reads_every_shape_of_field
        layouts_check_refuses_a_struct_without_a_layout
        layouts_check_refuses_a_layout_no_header_declares
        layouts_check_refuses_a_struct_defined_after_its_forward_typedef
        layouts_check_refuses_an_anonymous_typedef_struct
        layouts_check_refuses_a_struct_with_a_comment_before_its_brace
        layouts_check_refuses_a_forgotten_field
        layouts_check_refuses_fields_out_of_order
        layouts_check_refuses_fields_without_a_sizeof_line
        layouts_check_refuses_a_malformed_line)
    add_test(NAME ${scenario}
        COMMAND ${CMAKE_COMMAND} -DSCENARIO=${scenario} -DWORK=${CMAKE_CURRENT_BINARY_DIR}/layouts-work
            -DLAYOUTS=$<TARGET_FILE:layouts> -DHEADERS=${PROJECT_SOURCE_DIR}/include
            -DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/layouts/layouts_check.cmake
            -P ${CMAKE_CURRENT_SOURCE_DIR}/layouts/layouts_check_test.cmake)
endforeach()
