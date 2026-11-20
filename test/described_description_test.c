#include <stdio.h>
#include <string.h>

#include "allocation.h"
#include "check.h"
#include "described/described.h"
#include "described/loader.h"
#include "files.h"
#include "weivel/library.h"

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))
#define K(kind) WEIVEL_STANDARD_DESCRIBED_##kind

typedef struct expected_member {
    const char *name;
    size_t offset;
    size_t size;
    uint32_t kind;
    uint32_t element;
    const char *type;
    const char *count;
    const char *release;
} expected_member;

#define MEMBER(struct_type, member, member_kind, member_element, type_name, count_name, release_name) \
    {.name = #member,                                                                                   \
     .offset = offsetof(struct_type, member),                                                           \
     .size = sizeof(((struct_type *)0)->member),                                                        \
     .kind = K(member_kind),                                                                            \
     .element = K(member_element),                                                                      \
     .type = type_name,                                                                                 \
     .count = count_name,                                                                               \
     .release = release_name}

typedef struct expected_type {
    const char *name;
    size_t size;
    size_t first_shape;
    const expected_member *members;
    size_t member_count;
} expected_type;

#define TYPE(struct_type, shape_end, member_array) \
    {.name = #struct_type,                           \
     .size = sizeof(struct_type),                    \
     .first_shape = shape_end,                       \
     .members = member_array,                        \
     .member_count = COUNT_OF(member_array)}
#define ENDS_AT(struct_type, member) (offsetof(struct_type, member) + sizeof(((struct_type *)0)->member))

typedef struct expected_parameter {
    const char *name;
    uint32_t kind;
    uint32_t role;
    uint32_t element;
    const char *type;
    const char *count;
    const char *release;
} expected_parameter;

#define PARAMETER(parameter, parameter_kind, parameter_role, parameter_element, type_name, count_name, release_name) \
    {.name = parameter,                                                                                            \
     .kind = K(parameter_kind),                                                                                    \
     .role = K(parameter_role),                                                                                    \
     .element = K(parameter_element),                                                                              \
     .type = type_name,                                                                                            \
     .count = count_name,                                                                                          \
     .release = release_name}

typedef struct expected_operation {
    const char *name;
    size_t offset;
    uint32_t returns;
    const expected_parameter *parameters;
    size_t parameter_count;
} expected_operation;

#define OPERATION(definition, operation, returned, parameter_array) \
    {.name = #operation,                                             \
     .offset = offsetof(definition, operation),                      \
     .returns = K(returned),                                         \
     .parameters = parameter_array,                                  \
     .parameter_count = COUNT_OF(parameter_array)}

static const expected_member refusal_members[] = {
    MEMBER(weivel_standard_service_refusal, size, U32, NONE, NULL, NULL, NULL),
    MEMBER(weivel_standard_service_refusal, detail, TEXT, NONE, NULL, NULL, NULL),
    MEMBER(weivel_standard_service_refusal, fix, TEXT, NONE, NULL, NULL, NULL),
    MEMBER(weivel_standard_service_refusal, storage, STORAGE, NONE, NULL, NULL, NULL)};

static const expected_member point_members[] = {MEMBER(described_point, label, TEXT, NONE, NULL, NULL, NULL),
                                                MEMBER(described_point, x, I64, NONE, NULL, NULL, NULL),
                                                MEMBER(described_point, y, F64, NONE, NULL, NULL, NULL)};

static const expected_member sample_members[] = {
    MEMBER(described_sample, size, U32, NONE, NULL, NULL, NULL),
    MEMBER(described_sample, shade, U32, NONE, "described_shade", NULL, NULL),
    MEMBER(described_sample, tier, U32, NONE, "described_tier", NULL, NULL),
    MEMBER(described_sample, flags, U32, NONE, NULL, NULL, NULL),
    MEMBER(described_sample, big, U64, NONE, NULL, NULL, NULL),
    MEMBER(described_sample, delta, I64, NONE, NULL, NULL, NULL),
    MEMBER(described_sample, ratio, F64, NONE, NULL, NULL, NULL),
    MEMBER(described_sample, label, TEXT, NONE, NULL, NULL, NULL),
    MEMBER(described_sample, tags, TEXTS, NONE, NULL, "tag_count", NULL),
    MEMBER(described_sample, tag_count, SIZE, NONE, NULL, NULL, NULL),
    MEMBER(described_sample, bytes, BYTES, NONE, NULL, "length", NULL),
    MEMBER(described_sample, length, SIZE, NONE, NULL, NULL, NULL),
    MEMBER(described_sample, origin, STRUCT, NONE, "described_point", NULL, NULL),
    MEMBER(described_sample, best, POINTER, STRUCT, "described_point", NULL, NULL),
    MEMBER(described_sample, points, ARRAY, STRUCT, "described_point", "point_count", NULL),
    MEMBER(described_sample, point_count, SIZE, NONE, NULL, NULL, NULL),
    MEMBER(described_sample, numbers, ARRAY, U32, NULL, "number_count", NULL),
    MEMBER(described_sample, number_count, SIZE, NONE, NULL, NULL, NULL),
    MEMBER(described_sample, held, VALUE, NONE, NULL, NULL, NULL),
    MEMBER(described_sample, extra, POINTER, VALUE, NULL, NULL, NULL),
    MEMBER(described_sample, values, ARRAY, VALUE, NULL, "value_count", NULL),
    MEMBER(described_sample, value_count, SIZE, NONE, NULL, NULL, NULL)};

static const expected_member reflection_members[] = {
    MEMBER(described_reflection, size, U32, NONE, NULL, NULL, NULL),
    MEMBER(described_reflection, sample, STRUCT, NONE, "described_sample", NULL, NULL),
    MEMBER(described_reflection, storage, STORAGE, NONE, NULL, NULL, NULL)};

static const expected_member releases_members[] = {
    MEMBER(described_releases, size, U32, NONE, NULL, NULL, NULL),
    MEMBER(described_releases, reflections, U64, NONE, NULL, NULL, NULL),
    MEMBER(described_releases, trees, U64, NONE, NULL, NULL, NULL),
    MEMBER(described_releases, refusals, U64, NONE, NULL, NULL, NULL),
    MEMBER(described_releases, cursors, U64, NONE, NULL, NULL, NULL)};

static const expected_member mirror_members[] = {MEMBER(described_mirror, size, U32, NONE, NULL, NULL, NULL),
                                                 MEMBER(described_mirror, context, CONTEXT, NONE, NULL, NULL, NULL)};

static const expected_member visit_members[] = {
    MEMBER(described_visit, size, U32, NONE, NULL, NULL, NULL),
    MEMBER(described_visit, cursor, HANDLE, NONE, "described_cursor", NULL, "release_cursor"),
    MEMBER(described_visit, skip, U64, NONE, NULL, NULL, NULL),
    MEMBER(described_visit, steps, ARRAY, U64, NULL, "step_count", NULL),
    MEMBER(described_visit, step_count, SIZE, NONE, NULL, NULL, NULL)};

static const expected_member walker_members[] = {MEMBER(described_walker, size, U32, NONE, NULL, NULL, NULL),
                                                 MEMBER(described_walker, context, CONTEXT, NONE, NULL, NULL, NULL)};

static const expected_member loan_members[] = {
    MEMBER(described_loan, size, U32, NONE, NULL, NULL, NULL),
    MEMBER(described_loan, counter, DEFINITION, NONE, "described_counter", NULL, NULL)};

static const expected_member counter_members[] = {
    MEMBER(described_counter, size, U32, NONE, NULL, NULL, NULL),
    MEMBER(described_counter, context, CONTEXT, NONE, NULL, NULL, NULL)};

static const expected_type mirror_types[] = {
    TYPE(weivel_standard_service_refusal, ENDS_AT(weivel_standard_service_refusal, storage), refusal_members),
    TYPE(described_point, sizeof(described_point), point_members),
    TYPE(described_sample, ENDS_AT(described_sample, value_count), sample_members),
    TYPE(described_reflection, ENDS_AT(described_reflection, storage), reflection_members),
    TYPE(described_releases, ENDS_AT(described_releases, cursors), releases_members),
    TYPE(described_mirror, ENDS_AT(described_mirror, release_refusal), mirror_members)};

static const expected_type walker_types[] = {
    TYPE(weivel_standard_service_refusal, ENDS_AT(weivel_standard_service_refusal, storage), refusal_members),
    TYPE(described_visit, ENDS_AT(described_visit, step_count), visit_members),
    TYPE(described_loan, ENDS_AT(described_loan, counter), loan_members),
    TYPE(described_walker, ENDS_AT(described_walker, release_refusal), walker_members)};

static const expected_type counter_types[] = {
    TYPE(weivel_standard_service_refusal, ENDS_AT(weivel_standard_service_refusal, storage), refusal_members),
    TYPE(described_counter, ENDS_AT(described_counter, release_refusal), counter_members)};

static const expected_parameter reflect_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("sample", POINTER, INPUT, STRUCT, "described_sample", NULL, NULL),
    PARAMETER("reflection", POINTER, ANSWER, STRUCT, "described_reflection", NULL, "release_reflection"),
    PARAMETER("refusal", POINTER, REFUSAL, STRUCT, "weivel_standard_service_refusal", NULL, "release_refusal")};

static const expected_parameter tree_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("sample", POINTER, INPUT, STRUCT, "described_sample", NULL, NULL),
    PARAMETER("tree", VALUE, ANSWER, NONE, NULL, NULL, "release_tree"),
    PARAMETER("refusal", POINTER, REFUSAL, STRUCT, "weivel_standard_service_refusal", NULL, "release_refusal")};

static const expected_parameter find_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("key", TEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("found", TEXT, ANSWER, NONE, NULL, NULL, NULL),
    PARAMETER("refusal", POINTER, REFUSAL, STRUCT, "weivel_standard_service_refusal", NULL, "release_refusal")};

static const expected_parameter measure_parameters[] = {PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL),
                                                        PARAMETER("bytes", BYTES, INPUT, NONE, NULL, "length", NULL),
                                                        PARAMETER("length", SIZE, INPUT, NONE, NULL, NULL, NULL),
                                                        PARAMETER("sum", U64, ANSWER, NONE, NULL, NULL, NULL)};

static const expected_parameter negate_parameters[] = {PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL),
                                                       PARAMETER("number", I64, INPUT, NONE, NULL, NULL, NULL),
                                                       PARAMETER("negated", I64, ANSWER, NONE, NULL, NULL, NULL)};

static const expected_parameter halve_parameters[] = {PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL),
                                                      PARAMETER("number", F64, INPUT, NONE, NULL, NULL, NULL),
                                                      PARAMETER("half", F64, ANSWER, NONE, NULL, NULL, NULL)};

static const expected_parameter invert_parameters[] = {PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL),
                                                       PARAMETER("flags", U32, INPUT, NONE, NULL, NULL, NULL),
                                                       PARAMETER("inverted", U32, ANSWER, NONE, NULL, NULL, NULL)};

static const expected_parameter swap_shade_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("shade", U32, INPUT, NONE, "described_shade", NULL, NULL),
    PARAMETER("other", U32, ANSWER, NONE, "described_shade", NULL, NULL)};

static const expected_parameter grow_parameters[] = {PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL),
                                                     PARAMETER("amount", SIZE, INPUT, NONE, NULL, NULL, NULL),
                                                     PARAMETER("grown", SIZE, ANSWER, NONE, NULL, NULL, NULL)};

static const expected_parameter weigh_parameters[] = {PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL),
                                                      PARAMETER("value", POINTER, INPUT, VALUE, NULL, NULL, NULL),
                                                      PARAMETER("nodes", U64, ANSWER, NONE, NULL, NULL, NULL)};

static const expected_parameter count_releases_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("releases", POINTER, ANSWER, STRUCT, "described_releases", NULL, NULL)};

static const expected_parameter release_reflection_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("reflection", POINTER, ANSWER, STRUCT, "described_reflection", NULL, NULL)};

static const expected_parameter release_tree_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("tree", VALUE, ANSWER, NONE, NULL, NULL, NULL)};

static const expected_parameter release_refusal_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("refusal", POINTER, REFUSAL, STRUCT, "weivel_standard_service_refusal", NULL, NULL)};

static const expected_parameter open_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("label", TEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("start", U64, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("cursor", HANDLE, ANSWER, NONE, "described_cursor", NULL, "release_cursor"),
    PARAMETER("refusal", POINTER, REFUSAL, STRUCT, "weivel_standard_service_refusal", NULL, "release_refusal")};

static const expected_parameter use_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("cursor", HANDLE, INPUT, NONE, "described_cursor", NULL, "release_cursor"),
    PARAMETER("start", U64, ANSWER, NONE, NULL, NULL, NULL)};

static const expected_parameter visit_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("visit", POINTER, INPUT, STRUCT, "described_visit", NULL, NULL),
    PARAMETER("position", U64, ANSWER, NONE, NULL, NULL, NULL)};

static const expected_parameter release_cursor_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("cursor", HANDLE, INPUT, NONE, "described_cursor", NULL, "release_cursor")};

static const expected_operation mirror_operations[] = {
    OPERATION(described_mirror, reflect, STATUS, reflect_parameters),
    OPERATION(described_mirror, tree, STATUS, tree_parameters),
    OPERATION(described_mirror, find, STATUS, find_parameters),
    OPERATION(described_mirror, measure, STATUS, measure_parameters),
    OPERATION(described_mirror, negate, STATUS, negate_parameters),
    OPERATION(described_mirror, halve, STATUS, halve_parameters),
    OPERATION(described_mirror, invert, STATUS, invert_parameters),
    OPERATION(described_mirror, swap_shade, STATUS, swap_shade_parameters),
    OPERATION(described_mirror, grow, STATUS, grow_parameters),
    OPERATION(described_mirror, weigh, STATUS, weigh_parameters),
    OPERATION(described_mirror, count_releases, STATUS, count_releases_parameters),
    OPERATION(described_mirror, release_reflection, NOTHING, release_reflection_parameters),
    OPERATION(described_mirror, release_tree, NOTHING, release_tree_parameters),
    OPERATION(described_mirror, release_refusal, NOTHING, release_refusal_parameters)};

static const expected_parameter wait_parameters[] = {PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL),
                                                     PARAMETER("ticks", U64, INPUT, NONE, NULL, NULL, NULL),
                                                     PARAMETER("waited", U64, ANSWER, NONE, NULL, NULL, NULL)};

static const expected_parameter lend_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("loan", POINTER, ANSWER, STRUCT, "described_loan", NULL, NULL)};

static const expected_parameter add_parameters[] = {
    PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("amount", U64, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("total", U64, ANSWER, NONE, NULL, NULL, NULL),
    PARAMETER("refusal", POINTER, REFUSAL, STRUCT, "weivel_standard_service_refusal", NULL, "release_refusal")};

static const expected_operation walker_operations[] = {
    OPERATION(described_walker, open, STATUS, open_parameters),
    OPERATION(described_walker, use, STATUS, use_parameters),
    OPERATION(described_walker, visit, STATUS, visit_parameters),
    OPERATION(described_walker, release_cursor, NOTHING, release_cursor_parameters),
    OPERATION(described_walker, release_refusal, NOTHING, release_refusal_parameters),
    OPERATION(described_walker, wait, STATUS, wait_parameters),
    OPERATION(described_walker, lend, STATUS, lend_parameters)};

static const expected_operation counter_operations[] = {
    OPERATION(described_counter, add, STATUS, add_parameters),
    OPERATION(described_counter, release_refusal, NOTHING, release_refusal_parameters)};

typedef struct expected_description {
    const char *definition;
    const expected_type *types;
    size_t type_count;
    const expected_operation *operations;
    size_t operation_count;
    size_t enum_count;
    const char *const *blocking;
    size_t blocking_count;
    const struct expected_description *const *definitions;
    size_t definition_count;
} expected_description;

static int same_text(const char *actual, const char *expected) {
    return actual == NULL ? expected == NULL : expected != NULL && strcmp(actual, expected) == 0;
}

static void check_name(const char *actual, const char *expected, const char *where) {
    if (!same_text(actual, expected)) {
        fprintf(stderr, "%s: expected \"%s\", got \"%s\"\n", where, expected == NULL ? "(null)" : expected,
                actual == NULL ? "(null)" : actual);
        check_failures++;
    }
}

static void check_number(size_t actual, size_t expected, const char *where, const char *what) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %s %u, got %u\n", where, what, (unsigned)expected, (unsigned)actual);
        check_failures++;
    }
}

static void check_member(const weivel_standard_described_member *actual, const expected_member *expected,
                         const char *type) {
    char where[160];
    snprintf(where, sizeof where, "%s.%s", type, expected->name);
    check_name(actual->name, expected->name, where);
    check_number(actual->offset, expected->offset, where, "offset");
    check_number(actual->size, expected->size, where, "size");
    check_number(actual->kind, expected->kind, where, "kind");
    check_number(actual->element, expected->element, where, "element");
    check_name(actual->type, expected->type, where);
    check_name(actual->count, expected->count, where);
    check_name(actual->release, expected->release, where);
}

static void check_type(const weivel_standard_described_type *actual, const expected_type *expected) {
    size_t at;
    check_name(actual->name, expected->name, "a type's name");
    check_number(actual->size, expected->size, expected->name, "size");
    check_number(actual->first_shape, expected->first_shape, expected->name, "first shape");
    check_number(actual->member_count, expected->member_count, expected->name, "member count");
    for (at = 0; at < actual->member_count && at < expected->member_count; at++) {
        check_member(&actual->members[at], &expected->members[at], expected->name);
    }
}

static void check_parameter(const weivel_standard_described_parameter *actual, const expected_parameter *expected,
                            const char *operation) {
    char where[160];
    snprintf(where, sizeof where, "%s(%s)", operation, expected->name);
    check_name(actual->name, expected->name, where);
    check_number(actual->kind, expected->kind, where, "kind");
    check_number(actual->role, expected->role, where, "role");
    check_number(actual->element, expected->element, where, "element");
    check_name(actual->type, expected->type, where);
    check_name(actual->count, expected->count, where);
    check_name(actual->release, expected->release, where);
}

static void check_operation(const weivel_standard_described_operation *actual,
                            const expected_operation *expected) {
    size_t at;
    check_name(actual->name, expected->name, "an operation's name");
    check_number(actual->offset, expected->offset, expected->name, "offset");
    check_number(actual->returns, expected->returns, expected->name, "return");
    check_number(actual->parameter_count, expected->parameter_count, expected->name, "parameter count");
    for (at = 0; at < actual->parameter_count && at < expected->parameter_count; at++) {
        check_parameter(&actual->parameters[at], &expected->parameters[at], expected->name);
    }
}

static void check_description(const weivel_standard_description *actual, const expected_description *expected) {
    size_t at;
    CHECK(weivel_standard_description_check(actual, NULL) == WEIVEL_STATUS_OK);
    check_number(actual->blocking_count, expected->blocking_count, expected->definition, "blocking count");
    for (at = 0; at < actual->blocking_count && at < expected->blocking_count; at++) {
        check_name(actual->blocking[at], expected->blocking[at], "a blocking operation");
    }
    check_number(actual->definition_count, expected->definition_count, expected->definition, "definition count");
    for (at = 0; at < actual->definition_count && at < expected->definition_count; at++) {
        check_description(actual->definitions[at], expected->definitions[at]);
    }
    check_number(actual->size, sizeof(weivel_standard_description), expected->definition, "description size");
    check_name(actual->definition, expected->definition, "the definition");
    check_number(actual->type_count, expected->type_count, expected->definition, "type count");
    check_number(actual->operation_count, expected->operation_count, expected->definition, "operation count");
    check_number(actual->enum_count, expected->enum_count, expected->definition, "enum count");
    for (at = 0; at < actual->type_count && at < expected->type_count; at++) {
        check_type(&actual->types[at], &expected->types[at]);
    }
    for (at = 0; at < actual->operation_count && at < expected->operation_count; at++) {
        check_operation(&actual->operations[at], &expected->operations[at]);
    }
}

static void check_entry(const weivel_standard_described_enum *described, size_t at, const char *name,
                        uint32_t number) {
    CHECK(at < described->entry_count);
    if (at < described->entry_count) {
        check_name(described->entries[at].name, name, described->name);
        check_number(described->entries[at].number, number, name, "number");
    }
}

static void check_mirror_enums(const weivel_standard_description *mirror) {
    CHECK(mirror->enum_count == 2);
    if (mirror->enum_count != 2) {
        return;
    }
    check_name(mirror->enums[0].name, "described_shade", "the first enum");
    CHECK(mirror->enums[0].entry_count == 2);
    check_entry(&mirror->enums[0], 0, "DESCRIBED_SHADE_LIGHT", DESCRIBED_SHADE_LIGHT);
    check_entry(&mirror->enums[0], 1, "DESCRIBED_SHADE_DARK", DESCRIBED_SHADE_DARK);
    check_name(mirror->enums[1].name, "described_tier", "the second enum");
    CHECK(mirror->enums[1].entry_count == 2);
    check_entry(&mirror->enums[1], 0, "DESCRIBED_TIER_LOW", DESCRIBED_TIER_LOW);
    check_entry(&mirror->enums[1], 1, "DESCRIBED_TIER_HIGH", DESCRIBED_TIER_HIGH);
}

static const weivel_standard_description *resolve(described_library library, const char *symbol) {
    described_description_symbol describe = described_library_description(library, symbol);
    if (describe == NULL) {
        fprintf(stderr, "the library exports no %s\n", symbol);
        check_failures++;
        return NULL;
    }
    return describe();
}

static void describes_both_services_as_the_header_declares_them(described_library library) {
    static const expected_description mirror = {.definition = "described_mirror",
                                                 .types = mirror_types,
                                                 .type_count = COUNT_OF(mirror_types),
                                                 .operations = mirror_operations,
                                                 .operation_count = COUNT_OF(mirror_operations),
                                                 .enum_count = 2};
    static const expected_description counter = {.definition = "described_counter",
                                                 .types = counter_types,
                                                 .type_count = COUNT_OF(counter_types),
                                                 .operations = counter_operations,
                                                 .operation_count = COUNT_OF(counter_operations),
                                                 .enum_count = 0};
    static const expected_description *const walker_definitions[] = {&counter};
    static const char *const walker_blocking[] = {"wait"};
    static const expected_description walker = {.definition = "described_walker",
                                                .types = walker_types,
                                                .type_count = COUNT_OF(walker_types),
                                                .operations = walker_operations,
                                                .operation_count = COUNT_OF(walker_operations),
                                                .enum_count = 0,
                                                .blocking = walker_blocking,
                                                .blocking_count = COUNT_OF(walker_blocking),
                                                .definitions = walker_definitions,
                                                .definition_count = COUNT_OF(walker_definitions)};
    const weivel_standard_description *mirror_description = resolve(library, "described_mirror_description");
    const weivel_standard_description *walker_description = resolve(library, "described_walker_description");
    if (mirror_description != NULL) {
        check_description(mirror_description, &mirror);
        check_mirror_enums(mirror_description);
    }
    if (walker_description != NULL) {
        check_description(walker_description, &walker);
        CHECK(walker_description->enums == NULL);
    }
}

static const weivel_standard_described_member *visit_member(const weivel_standard_description *description,
                                                            const char *name) {
    size_t type;
    size_t member;
    for (type = 0; type < description->type_count; type++) {
        if (strcmp(description->types[type].name, "described_visit") != 0) {
            continue;
        }
        for (member = 0; member < description->types[type].member_count; member++) {
            if (strcmp(description->types[type].members[member].name, name) == 0) {
                return &description->types[type].members[member];
            }
        }
    }
    return NULL;
}

/** The check cannot see where a member really is, so the offset fault passes it and only the layout finds it. */
static void checks_an_offset_fault_only_against_the_layout(const weivel_standard_description *broken) {
    const weivel_standard_described_member *skip;
    CHECK(weivel_standard_description_check(broken, NULL) == WEIVEL_STATUS_OK);
    skip = visit_member(broken, "skip");
    CHECK(skip != NULL && skip->offset != offsetof(described_visit, skip));
}

static void refuses_a_definition_no_nested_description_defines(const weivel_standard_description *broken) {
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    CHECK(weivel_standard_description_check(broken, &refusal) == WEIVEL_STATUS_MALFORMED);
    check_text(refusal.path, "types[2].members[1].type", "the definition fault's path");
    weivel_standard_refusal_release(&refusal);
}

static void refuses_a_count_naming_no_member(const weivel_standard_description *broken) {
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    CHECK(weivel_standard_description_check(broken, &refusal) == WEIVEL_STATUS_MALFORMED);
    check_text(refusal.path, "types[1].members[3].count", "the count fault's path");
    weivel_standard_refusal_release(&refusal);
}

static int read_declaration(const char *name, weivel_standard_library *declaration) {
    char path[512];
    char *bytes = NULL;
    size_t length = 0;
    weivel_status status;
    snprintf(path, sizeof path, "%s/%s", WEIVEL_DESCRIBED_DIRECTORY, name);
    CHECK(weivel_standard_files_read(path, &bytes, &length) == WEIVEL_STANDARD_FILES_DONE);
    if (bytes == NULL) {
        return 0;
    }
    status = weivel_standard_read_library(bytes, length, declaration, NULL);
    weivel_standard_memory_release(bytes);
    CHECK(status == WEIVEL_STATUS_OK);
    return status == WEIVEL_STATUS_OK;
}

static void check_service(const weivel_standard_library *declaration, size_t at, const char *id,
                          const char *definition, const char *description) {
    const weivel_standard_library_service *service = &declaration->services[at];
    check_name(service->id, id, "a service's id");
    CHECK(service->function_count == 1);
    if (service->function_count == 1) {
        check_name(service->functions[0].symbol, definition, id);
        CHECK(service->functions[0].kind == WEIVEL_STANDARD_FUNCTION_DEFINITION);
    }
    check_name(declaration->descriptions[at], description, id);
}

/** Reads a declaration and answers the description its walker names, after checking every symbol it names exists. */
static const weivel_standard_description *walker_declared_by(described_library library, const char *name,
                                                             const char *walker_description) {
    weivel_standard_library declaration = WEIVEL_INIT(weivel_standard_library);
    const weivel_standard_description *walker = NULL;
    if (!read_declaration(name, &declaration)) {
        return NULL;
    }
    check_name(declaration.import_name, "described", name);
    CHECK(declaration.release == NULL);
    CHECK(declaration.service_count == 2 && declaration.descriptions != NULL);
    if (declaration.service_count == 2 && declaration.descriptions != NULL) {
        check_service(&declaration, 0, "described-mirror", "described_mirror_definition",
                      "described_mirror_description");
        check_service(&declaration, 1, "described-walker", "described_walker_definition", walker_description);
        CHECK(described_library_definition(library, declaration.services[0].functions[0].symbol) != NULL);
        CHECK(described_library_definition(library, declaration.services[1].functions[0].symbol) != NULL);
        walker = resolve(library, declaration.descriptions[1]);
    }
    weivel_standard_library_release(&declaration);
    return walker;
}

static void declares_both_services_and_each_broken_variant(described_library library) {
    const weivel_standard_description *walker =
        walker_declared_by(library, "library.json", "described_walker_description");
    const weivel_standard_description *offset_fault =
        walker_declared_by(library, "library-broken-offset.json", "described_broken_offset_description");
    const weivel_standard_description *count_fault =
        walker_declared_by(library, "library-broken-count.json", "described_broken_count_description");
    const weivel_standard_description *definition_fault =
        walker_declared_by(library, "library-broken-definition.json", "described_broken_definition_description");
    CHECK(walker != NULL && weivel_standard_description_check(walker, NULL) == WEIVEL_STATUS_OK);
    if (offset_fault != NULL) {
        checks_an_offset_fault_only_against_the_layout(offset_fault);
    }
    if (count_fault != NULL) {
        refuses_a_count_naming_no_member(count_fault);
    }
    if (definition_fault != NULL) {
        refuses_a_definition_no_nested_description_defines(definition_fault);
    }
}

int main(int argument_count, char **arguments) {
    described_library library;
    CHECK(argument_count == 2);
    if (argument_count != 2 || !described_library_open(arguments[1], &library)) {
        fprintf(stderr, "the described library cannot be loaded\n");
        return 1;
    }
    describes_both_services_as_the_header_declares_them(library);
    declares_both_services_and_each_broken_variant(library);
    CHECK(described_library_close(library));
    return check_summary("described_description_test");
}
