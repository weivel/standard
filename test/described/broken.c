/**
 * @file
 * Three descriptions of the walker, written by hand as copies of the one weivel-describe writes, each with one fault,
 * so a host can prove it refuses to bind any of them.
 */

#include <stddef.h>
#include <stdint.h>

#include "described.h"

#define MEMBER_OF(struct_type, member, member_kind, member_element, type_name, count_name, release_name) \
    {.name = #member,                                                                                     \
     .offset = offsetof(struct_type, member),                                                             \
     .size = sizeof(((struct_type *)0)->member),                                                          \
     .kind = WEIVEL_STANDARD_DESCRIBED_##member_kind,                                                     \
     .element = WEIVEL_STANDARD_DESCRIBED_##member_element,                                               \
     .type = type_name,                                                                                   \
     .count = count_name,                                                                                 \
     .release = release_name}

#define PARAMETER(parameter, parameter_kind, parameter_role, parameter_element, type_name, release_name) \
    {.name = parameter,                                                                                 \
     .kind = WEIVEL_STANDARD_DESCRIBED_##parameter_kind,                                                \
     .role = WEIVEL_STANDARD_DESCRIBED_##parameter_role,                                                \
     .element = WEIVEL_STANDARD_DESCRIBED_##parameter_element,                                          \
     .type = type_name,                                                                                 \
     .count = NULL,                                                                                     \
     .release = release_name}

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

#define TYPE_OF(struct_type, last, member_array)                                          \
    {.name = #struct_type,                                                                \
     .size = sizeof(struct_type),                                                         \
     .first_shape = offsetof(struct_type, last) + sizeof(((struct_type *)0)->last),       \
     .members = member_array,                                                             \
     .member_count = COUNT_OF(member_array)}

#define OPERATION_OF(definition, operation, returned, parameter_array) \
    {.name = #operation,                                               \
     .offset = offsetof(definition, operation),                        \
     .returns = WEIVEL_STANDARD_DESCRIBED_##returned,                  \
     .parameters = parameter_array,                                    \
     .parameter_count = COUNT_OF(parameter_array)}

#define OPERATION(operation, returned, parameter_array) \
    OPERATION_OF(described_walker, operation, returned, parameter_array)

static const weivel_standard_described_member refusal_members[] = {
    MEMBER_OF(weivel_standard_service_refusal, size, U32, NONE, NULL, NULL, NULL),
    MEMBER_OF(weivel_standard_service_refusal, detail, TEXT, NONE, NULL, NULL, NULL),
    MEMBER_OF(weivel_standard_service_refusal, fix, TEXT, NONE, NULL, NULL, NULL),
    MEMBER_OF(weivel_standard_service_refusal, storage, STORAGE, NONE, NULL, NULL, NULL)};

static const weivel_standard_described_member offset_visit_members[] = {
    MEMBER_OF(described_visit, size, U32, NONE, NULL, NULL, NULL),
    MEMBER_OF(described_visit, cursor, HANDLE, NONE, "described_cursor", NULL, "release_cursor"),
    {.name = "skip",
     .offset = offsetof(described_visit, skip) + sizeof(uint32_t),
     .size = sizeof(((described_visit *)0)->skip),
     .kind = WEIVEL_STANDARD_DESCRIBED_U64,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL},
    MEMBER_OF(described_visit, steps, ARRAY, U64, NULL, "step_count", NULL),
    MEMBER_OF(described_visit, step_count, SIZE, NONE, NULL, NULL, NULL)};

static const weivel_standard_described_member count_visit_members[] = {
    MEMBER_OF(described_visit, size, U32, NONE, NULL, NULL, NULL),
    MEMBER_OF(described_visit, cursor, HANDLE, NONE, "described_cursor", NULL, "release_cursor"),
    MEMBER_OF(described_visit, skip, U64, NONE, NULL, NULL, NULL),
    MEMBER_OF(described_visit, steps, ARRAY, U64, NULL, "step_total", NULL),
    MEMBER_OF(described_visit, step_count, SIZE, NONE, NULL, NULL, NULL)};

static const weivel_standard_described_member visit_members[] = {
    MEMBER_OF(described_visit, size, U32, NONE, NULL, NULL, NULL),
    MEMBER_OF(described_visit, cursor, HANDLE, NONE, "described_cursor", NULL, "release_cursor"),
    MEMBER_OF(described_visit, skip, U64, NONE, NULL, NULL, NULL),
    MEMBER_OF(described_visit, steps, ARRAY, U64, NULL, "step_count", NULL),
    MEMBER_OF(described_visit, step_count, SIZE, NONE, NULL, NULL, NULL)};

static const weivel_standard_described_member loan_members[] = {
    MEMBER_OF(described_loan, size, U32, NONE, NULL, NULL, NULL),
    MEMBER_OF(described_loan, counter, DEFINITION, NONE, "described_counter", NULL, NULL)};

static const weivel_standard_described_member counter_members[] = {
    MEMBER_OF(described_counter, size, U32, NONE, NULL, NULL, NULL),
    MEMBER_OF(described_counter, context, CONTEXT, NONE, NULL, NULL, NULL)};

static const weivel_standard_described_parameter add_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL), PARAMETER("amount", U64, INPUT, NONE, NULL, NULL),
    PARAMETER("total", U64, ANSWER, NONE, NULL, NULL)};

static const weivel_standard_described_operation counter_operations[] = {
    OPERATION_OF(described_counter, add, STATUS, add_parameters)};

static const weivel_standard_described_type counter_types[] = {TYPE_OF(described_counter, add, counter_members)};

static const weivel_standard_description counter = {.size = (uint32_t)sizeof(weivel_standard_description),
                                                     .definition = "described_counter",
                                                     .types = counter_types,
                                                     .type_count = COUNT_OF(counter_types),
                                                     .enums = NULL,
                                                     .enum_count = 0,
                                                     .operations = counter_operations,
                                                     .operation_count = COUNT_OF(counter_operations),
                                                     .blocking = NULL,
                                                     .blocking_count = 0,
                                                     .definitions = NULL,
                                                     .definition_count = 0};

static const weivel_standard_description *const definitions[] = {&counter};

static const char *const blocking[] = {"wait"};

static const weivel_standard_described_member walker_members[] = {
    MEMBER_OF(described_walker, size, U32, NONE, NULL, NULL, NULL),
    MEMBER_OF(described_walker, context, CONTEXT, NONE, NULL, NULL, NULL)};

static const weivel_standard_described_parameter open_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL),
    PARAMETER("label", TEXT, INPUT, NONE, NULL, NULL),
    PARAMETER("start", U64, INPUT, NONE, NULL, NULL),
    PARAMETER("cursor", HANDLE, ANSWER, NONE, "described_cursor", "release_cursor"),
    PARAMETER("refusal", POINTER, REFUSAL, STRUCT, "weivel_standard_service_refusal", "release_refusal")};

static const weivel_standard_described_parameter use_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL),
    PARAMETER("cursor", HANDLE, INPUT, NONE, "described_cursor", "release_cursor"),
    PARAMETER("start", U64, ANSWER, NONE, NULL, NULL)};

static const weivel_standard_described_parameter visit_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL),
    PARAMETER("visit", POINTER, INPUT, STRUCT, "described_visit", NULL),
    PARAMETER("position", U64, ANSWER, NONE, NULL, NULL)};

static const weivel_standard_described_parameter release_cursor_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL),
    PARAMETER("cursor", HANDLE, INPUT, NONE, "described_cursor", "release_cursor")};

static const weivel_standard_described_parameter release_refusal_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL),
    PARAMETER("refusal", POINTER, REFUSAL, STRUCT, "weivel_standard_service_refusal", NULL)};

static const weivel_standard_described_parameter wait_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL), PARAMETER("ticks", U64, INPUT, NONE, NULL, NULL),
    PARAMETER("waited", U64, ANSWER, NONE, NULL, NULL)};

static const weivel_standard_described_parameter lend_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL),
    PARAMETER("loan", POINTER, ANSWER, STRUCT, "described_loan", NULL)};

static const weivel_standard_described_operation operations[] = {
    OPERATION(open, STATUS, open_parameters),
    OPERATION(use, STATUS, use_parameters),
    OPERATION(visit, STATUS, visit_parameters),
    OPERATION(release_cursor, NOTHING, release_cursor_parameters),
    OPERATION(release_refusal, NOTHING, release_refusal_parameters),
    OPERATION(wait, STATUS, wait_parameters),
    OPERATION(lend, STATUS, lend_parameters)};

static const weivel_standard_described_type offset_types[] = {
    TYPE_OF(weivel_standard_service_refusal, storage, refusal_members),
    TYPE_OF(described_visit, step_count, offset_visit_members), TYPE_OF(described_loan, counter, loan_members),
    TYPE_OF(described_walker, release_refusal, walker_members)};

static const weivel_standard_described_type count_types[] = {
    TYPE_OF(weivel_standard_service_refusal, storage, refusal_members),
    TYPE_OF(described_visit, step_count, count_visit_members), TYPE_OF(described_loan, counter, loan_members),
    TYPE_OF(described_walker, release_refusal, walker_members)};

static const weivel_standard_described_type definition_types[] = {
    TYPE_OF(weivel_standard_service_refusal, storage, refusal_members),
    TYPE_OF(described_visit, step_count, visit_members), TYPE_OF(described_loan, counter, loan_members),
    TYPE_OF(described_walker, release_refusal, walker_members)};

#define WALKER_OF(type_array, nested_array, nested_count)          \
    {.size = (uint32_t)sizeof(weivel_standard_description),        \
     .definition = "described_walker",                             \
     .types = type_array,                                          \
     .type_count = COUNT_OF(type_array),                           \
     .enums = NULL,                                                \
     .enum_count = 0,                                              \
     .operations = operations,                                     \
     .operation_count = COUNT_OF(operations),                      \
     .blocking = blocking,                                         \
     .blocking_count = COUNT_OF(blocking),                         \
     .definitions = nested_array,                                  \
     .definition_count = nested_count}

static const weivel_standard_description broken_offset = WALKER_OF(offset_types, definitions, COUNT_OF(definitions));

static const weivel_standard_description broken_count = WALKER_OF(count_types, definitions, COUNT_OF(definitions));

static const weivel_standard_description broken_definition = WALKER_OF(definition_types, NULL, 0);

/**
 * Places `skip` four bytes past where described_visit holds it, still inside the struct, so
 * weivel_standard_description_check passes it and only a binding comparing its own layout finds it.
 */
DESCRIBED_API const weivel_standard_description *described_broken_offset_description(void);

/**
 * Counts `steps` by `step_total`, which no member of described_visit is, so weivel_standard_description_check refuses
 * it.
 */
DESCRIBED_API const weivel_standard_description *described_broken_count_description(void);

/**
 * Holds no nested description, so the loan's `counter` names a definition `definitions` does not describe, and
 * weivel_standard_description_check refuses it.
 */
DESCRIBED_API const weivel_standard_description *described_broken_definition_description(void);

const weivel_standard_description *described_broken_offset_description(void) {
    return &broken_offset;
}

const weivel_standard_description *described_broken_count_description(void) {
    return &broken_count;
}

const weivel_standard_description *described_broken_definition_description(void) {
    return &broken_definition;
}

