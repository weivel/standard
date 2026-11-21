#include <stdio.h>
#include <string.h>

#include "check.h"
#include "reader.h"

#define K(name) WEIVEL_STANDARD_DESCRIBED_##name
#define NO K(NONE)
#define MEMBER K(NO_ROLE)
#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))
#define FIXTURE(name) WEIVEL_DESCRIBE_FIXTURES "/" name
#define REFUSED(name) WEIVEL_DESCRIBE_FIXTURES "/refused/" name

typedef struct expected_item {
    const char *name;
    uint32_t kind;
    uint32_t role;
    uint32_t element;
    const char *type;
    const char *count;
    const char *release;
} expected_item;

typedef struct expected_operation {
    const char *name;
    uint32_t returns;
    const char *frees;
    const expected_item *parameters;
    size_t parameter_count;
} expected_operation;

static int same_text(const char *actual, const char *expected) {
    return actual == NULL || expected == NULL ? actual == expected : strcmp(actual, expected) == 0;
}

static void check_nullable(const char *actual, const char *expected, const char *label, const char *field) {
    if (!same_text(actual, expected)) {
        fprintf(stderr, "%s.%s: expected %s, got %s\n", label, field, expected == NULL ? "NULL" : expected,
                actual == NULL ? "NULL" : actual);
        check_failures++;
    }
}

static void check_number(uint32_t actual, uint32_t expected, const char *label, const char *field) {
    if (actual != expected) {
        fprintf(stderr, "%s.%s: expected %u, got %u\n", label, field, (unsigned)expected, (unsigned)actual);
        check_failures++;
    }
}

static void check_items(const char *label, const describe_item *items, size_t count, const expected_item *expected,
                        size_t expected_count) {
    size_t at;
    if (count != expected_count) {
        fprintf(stderr, "%s: expected %u items, got %u\n", label, (unsigned)expected_count, (unsigned)count);
        check_failures++;
        return;
    }
    for (at = 0; at < count; at++) {
        const describe_item *item = &items[at];
        const expected_item *wanted = &expected[at];
        char path[160];
        snprintf(path, sizeof path, "%.60s[%u]", label, (unsigned)at);
        check_nullable(item->name, wanted->name, path, "name");
        check_number(item->kind, wanted->kind, path, "kind");
        check_number(item->role, wanted->role, path, "role");
        check_number(item->element, wanted->element, path, "element");
        check_nullable(item->type, wanted->type, path, "type");
        check_nullable(item->count, wanted->count, path, "count");
        check_nullable(item->release, wanted->release, path, "release");
    }
}

static const describe_type *type_checked(const describe_model *model, const char *name) {
    const describe_type *type = describe_model_type(model, name);
    if (type == NULL) {
        fprintf(stderr, "no type %s\n", name);
        check_failures++;
    }
    return type;
}

static void check_members(const describe_model *model, const char *name, const expected_item *expected,
                          size_t count) {
    const describe_type *type = type_checked(model, name);
    if (type != NULL) {
        check_items(name, type->members, type->member_count, expected, count);
    }
}

static void check_operations(const describe_model *model, const char *name, const expected_operation *expected,
                             size_t count) {
    const describe_type *type = type_checked(model, name);
    size_t at;
    if (type == NULL) {
        return;
    }
    CHECK(type->definition);
    if (type->operation_count != count) {
        fprintf(stderr, "%s: expected %u operations, got %u\n", name, (unsigned)count,
                (unsigned)type->operation_count);
        check_failures++;
        return;
    }
    for (at = 0; at < count; at++) {
        const describe_operation *operation = &type->operations[at];
        check_nullable(operation->name, expected[at].name, name, "operations.name");
        check_number(operation->returns, expected[at].returns, expected[at].name, "returns");
        check_nullable(operation->frees, expected[at].frees, expected[at].name, "frees");
        check_items(expected[at].name, operation->parameters, operation->parameter_count, expected[at].parameters,
                    expected[at].parameter_count);
    }
}

static void check_shape(const describe_model *model, const char *name, int sized, const char *first_shape) {
    const describe_type *type = type_checked(model, name);
    if (type != NULL) {
        check_number((uint32_t)type->sized, (uint32_t)sized, name, "sized");
        check_nullable(type->first_shape, first_shape, name, "first_shape");
    }
}

static describe_outcome read_all(describe_model *model, const char *const *paths, size_t count,
                                 const describe_names *names) {
    describe_refusal refusal;
    describe_outcome outcome = describe_read(model, paths, count, names, &refusal);
    if (outcome == DESCRIBE_REFUSED) {
        fprintf(stderr, "refused: %s:%u: %s\n", refusal.file == NULL ? "" : refusal.file, (unsigned)refusal.line,
                refusal.message);
        describe_refusal_release(&refusal);
    }
    return outcome;
}

static int read_every_form(describe_model *model) {
    const char *paths[] = {WEIVEL_DESCRIBE_SERVICES_HEADER, FIXTURE("every_form.h")};
    describe_names names = describe_names_of_the_standard();
    describe_outcome outcome = read_all(model, paths, COUNT_OF(paths), &names);
    CHECK(outcome == DESCRIBE_DONE);
    return outcome == DESCRIBE_DONE;
}

#define CONTEXT_PARAMETER {"context", K(CONTEXT), K(INPUT), NO, NULL, NULL, NULL}
#define REFUSAL_PARAMETER                                                                                   \
    {"refusal", K(POINTER), K(REFUSAL), K(STRUCT), "weivel_standard_service_refusal", NULL, "release_refusal"}
#define RELEASED_REFUSAL {"refusal", K(POINTER), K(REFUSAL), K(STRUCT), "weivel_standard_service_refusal", NULL, NULL}

static const expected_item PROBE_INPUT[] = {
    {"size", K(U32), MEMBER, NO, NULL, NULL, NULL},
    {"mode", K(U32), MEMBER, NO, "probe_mode", NULL, NULL},
    {"delta", K(I64), MEMBER, NO, NULL, NULL, NULL},
    {"ratio", K(F64), MEMBER, NO, NULL, NULL, NULL},
    {"label", K(TEXT), MEMBER, NO, NULL, NULL, NULL},
    {"tags", K(TEXTS), MEMBER, NO, NULL, "tag_count", NULL},
    {"tag_count", K(SIZE), MEMBER, NO, NULL, NULL, NULL},
    {"bytes", K(BYTES), MEMBER, NO, NULL, "length", NULL},
    {"length", K(SIZE), MEMBER, NO, NULL, NULL, NULL},
    {"first", K(STRUCT), MEMBER, NO, "probe_item", NULL, NULL},
    {"best", K(POINTER), MEMBER, K(STRUCT), "probe_item", NULL, NULL},
    {"items", K(ARRAY), MEMBER, K(STRUCT), "probe_item", "item_count", NULL},
    {"item_count", K(SIZE), MEMBER, NO, NULL, NULL, NULL},
    {"numbers", K(ARRAY), MEMBER, K(U32), NULL, "number_count", NULL},
    {"number_count", K(SIZE), MEMBER, NO, NULL, NULL, NULL},
    {"extra", K(POINTER), MEMBER, K(VALUE), NULL, NULL, NULL},
    {"previous", K(VALUE), MEMBER, NO, NULL, NULL, NULL}};

static const expected_item PROBE_ANSWER[] = {{"size", K(U32), MEMBER, NO, NULL, NULL, NULL},
                                             {"walk", K(HANDLE), MEMBER, NO, "probe_handle", NULL, "release_walk"},
                                             {"storage", K(STORAGE), MEMBER, NO, NULL, NULL, NULL}};

static const expected_item PROBE_DEFINITION[] = {{"size", K(U32), MEMBER, NO, NULL, NULL, NULL},
                                                 {"context", K(CONTEXT), MEMBER, NO, NULL, NULL, NULL}};

static void reads_a_member_of_every_kind(void) {
    describe_model model;
    static const expected_item item[] = {{"label", K(TEXT), MEMBER, NO, NULL, NULL, NULL},
                                         {"weight", K(U64), MEMBER, NO, NULL, NULL, NULL}};
    if (!read_every_form(&model)) {
        return;
    }
    check_members(&model, "probe_item", item, COUNT_OF(item));
    check_members(&model, "probe_input", PROBE_INPUT, COUNT_OF(PROBE_INPUT));
    check_members(&model, "probe_answer", PROBE_ANSWER, COUNT_OF(PROBE_ANSWER));
    check_members(&model, "probe_definition", PROBE_DEFINITION, COUNT_OF(PROBE_DEFINITION));
    check_shape(&model, "probe_item", 0, NULL);
    check_shape(&model, "probe_input", 1, "previous");
    check_shape(&model, "probe_answer", 1, "storage");
    check_shape(&model, "probe_definition", 1, "release_refusal");
    CHECK(describe_model_handle(&model, "probe_handle") != NULL);
    check_nullable(describe_model_handle(&model, "probe_handle")->release, "release_walk", "probe_handle", "release");
    describe_model_release(&model);
}

static const expected_item RUN[] = {
    CONTEXT_PARAMETER,
    {"input", K(POINTER), K(INPUT), K(STRUCT), "probe_input", NULL, NULL},
    {"answer", K(POINTER), K(ANSWER), K(STRUCT), "probe_answer", NULL, "release_answer"},
    REFUSAL_PARAMETER};

static const expected_item GET[] = {CONTEXT_PARAMETER,
                                    {"key", K(TEXT), K(INPUT), NO, NULL, NULL, NULL},
                                    {"bytes", K(BYTES), K(INPUT), NO, NULL, "length", NULL},
                                    {"length", K(SIZE), K(INPUT), NO, NULL, NULL, NULL},
                                    {"directory", K(TEXT), K(ANSWER), NO, NULL, NULL, NULL},
                                    REFUSAL_PARAMETER};

static const expected_item READ[] = {CONTEXT_PARAMETER,
                                     {"query", K(POINTER), K(INPUT), K(VALUE), NULL, NULL, NULL},
                                     {"tree", K(VALUE), K(ANSWER), NO, NULL, NULL, "release_tree"},
                                     REFUSAL_PARAMETER};

static const expected_item OPEN[] = {CONTEXT_PARAMETER,
                                     {"items", K(ARRAY), K(INPUT), K(STRUCT), "probe_item", "item_count", NULL},
                                     {"item_count", K(SIZE), K(INPUT), NO, NULL, NULL, NULL},
                                     {"walk", K(HANDLE), K(ANSWER), NO, "probe_handle", NULL, "release_walk"},
                                     {"total", K(U64), K(ANSWER), NO, NULL, NULL, NULL}};

static const expected_item STEP[] = {CONTEXT_PARAMETER,
                                     {"walk", K(HANDLE), K(INPUT), NO, "probe_handle", NULL, "release_walk"},
                                     REFUSAL_PARAMETER};

static const expected_item RELEASE_ANSWER[] = {
    CONTEXT_PARAMETER, {"answer", K(POINTER), K(ANSWER), K(STRUCT), "probe_answer", NULL, NULL}};
static const expected_item RELEASE_TREE[] = {CONTEXT_PARAMETER, {"tree", K(VALUE), K(ANSWER), NO, NULL, NULL, NULL}};
static const expected_item RELEASE_WALK[] = {
    CONTEXT_PARAMETER, {"walk", K(HANDLE), K(INPUT), NO, "probe_handle", NULL, "release_walk"}};
static const expected_item RELEASE_REFUSAL[] = {CONTEXT_PARAMETER, RELEASED_REFUSAL};

#define OPERATION(name, returns, frees, parameters) {name, K(returns), frees, parameters, COUNT_OF(parameters)}

static void reads_an_operation_of_every_form(void) {
    describe_model model;
    static const expected_operation operations[] = {
        OPERATION("run", STATUS, NULL, RUN),
        OPERATION("get", STATUS, NULL, GET),
        OPERATION("read", STATUS, NULL, READ),
        OPERATION("open", STATUS, NULL, OPEN),
        OPERATION("step", STATUS, NULL, STEP),
        OPERATION("release_answer", NOTHING, "probe_answer", RELEASE_ANSWER),
        OPERATION("release_tree", NOTHING, "weivel_standard_value_tree", RELEASE_TREE),
        OPERATION("release_walk", NOTHING, "probe_handle", RELEASE_WALK),
        OPERATION("release_refusal", NOTHING, "weivel_standard_service_refusal", RELEASE_REFUSAL)};
    if (!read_every_form(&model)) {
        return;
    }
    check_operations(&model, "probe_definition", operations, COUNT_OF(operations));
    describe_model_release(&model);
}

static void keeps_each_parameter_doc(void) {
    describe_model model;
    const describe_operation *open;
    if (!read_every_form(&model)) {
        return;
    }
    open = describe_type_operation(describe_model_type(&model, "probe_definition"), "open");
    check_text(open->doc, "Opens a walk over `items`. @param items read during the call only. @param walk the "
                          "consumer's empty handle. Required. @param total answers how many there were.",
               "open.doc");
    check_text(open->parameters[1].doc, "read during the call only.", "open.items.doc");
    check_text(open->parameters[3].doc, "the consumer's empty handle. Required.", "open.walk.doc");
    check_nullable(open->parameters[2].doc, NULL, "open.item_count", "doc");
    check_text(describe_model_type(&model, "probe_input")->members[1].doc, "A probe_mode.", "probe_input.mode.doc");
    describe_model_release(&model);
}

static void check_where(describe_location where, const char *file, uint32_t line, const char *label) {
    check_nullable(where.file, file, label, "file");
    check_number(where.line, line, label, "line");
}

static void records_where_each_item_is_declared(void) {
    describe_model model;
    const describe_type *input;
    const describe_type *definition;
    const describe_enum *mode;
    if (!read_every_form(&model)) {
        return;
    }
    input = describe_model_type(&model, "probe_input");
    definition = describe_model_type(&model, "probe_definition");
    mode = describe_model_enum(&model, "probe_mode");
    check_where(input->where, FIXTURE("every_form.h"), 32, "probe_input");
    check_where(input->members[1].where, FIXTURE("every_form.h"), 35, "probe_input.mode");
    check_where(definition->operations[1].where, FIXTURE("every_form.h"), 66, "get");
    check_where(definition->operations[1].parameters[5].where, FIXTURE("every_form.h"), 67, "get.refusal");
    check_where(mode->where, FIXTURE("every_form.h"), 23, "probe_mode");
    check_where(mode->entries[1].where, FIXTURE("every_form.h"), 23, "PROBE_SLOW");
    check_where(describe_model_handle(&model, "probe_handle")->where, FIXTURE("every_form.h"), 20, "probe_handle");
    check_where(describe_model_type(&model, "weivel_standard_service_refusal")->where,
                WEIVEL_DESCRIBE_SERVICES_HEADER, 76, "weivel_standard_service_refusal");
    describe_model_release(&model);
}

static void check_reach(const describe_model *model, const char *definition, const char *const *types,
                        size_t type_count, const char *const *enums, size_t enum_count) {
    describe_reach reach;
    describe_refusal refusal;
    size_t at;
    if (describe_reach_of(model, definition, &reach, &refusal) != DESCRIBE_DONE) {
        fprintf(stderr, "%s: reach refused: %s\n", definition, refusal.message == NULL ? "" : refusal.message);
        describe_refusal_release(&refusal);
        check_failures++;
        return;
    }
    check_nullable(reach.definition->name, definition, definition, "reach.definition");
    check_number((uint32_t)reach.type_count, (uint32_t)type_count, definition, "reach.type_count");
    for (at = 0; at < reach.type_count && at < type_count; at++) {
        check_nullable(reach.types[at]->name, types[at], definition, "reach.types");
    }
    check_number((uint32_t)reach.enum_count, (uint32_t)enum_count, definition, "reach.enum_count");
    for (at = 0; at < reach.enum_count && at < enum_count; at++) {
        check_nullable(reach.enums[at]->name, enums[at], definition, "reach.enums");
    }
    describe_reach_release(&reach);
}

static void reaches_what_a_definition_uses(void) {
    describe_model model;
    static const char *const types[] = {"weivel_standard_service_refusal", "probe_item", "probe_input", "probe_answer",
                                        "probe_definition"};
    static const char *const enums[] = {"probe_mode"};
    if (!read_every_form(&model)) {
        return;
    }
    check_reach(&model, "probe_definition", types, COUNT_OF(types), enums, COUNT_OF(enums));
    describe_model_release(&model);
}

static void numbers_enum_entries(void) {
    describe_model model;
    const char *paths[] = {FIXTURE("enums.h")};
    describe_names names = describe_names_of_the_standard();
    static const char *const entries[] = {"PROBE_ZERO", "PROBE_ONE", "PROBE_TEN", "PROBE_ELEVEN", "PROBE_HEX",
                                          "PROBE_LARGEST"};
    static const uint32_t numbers[] = {0, 1, 10, 11, 32, 4294967295u};
    const describe_enum *described;
    size_t at;
    if (read_all(&model, paths, COUNT_OF(paths), &names) != DESCRIBE_DONE) {
        CHECK(0);
        return;
    }
    described = describe_model_enum(&model, "probe_numbers");
    CHECK(described != NULL && described->entry_count == COUNT_OF(entries));
    for (at = 0; described != NULL && at < described->entry_count && at < COUNT_OF(entries); at++) {
        check_nullable(described->entries[at].name, entries[at], "probe_numbers", "name");
        check_number(described->entries[at].number, numbers[at], entries[at], "number");
    }
    check_text(described == NULL ? NULL : described->doc, "Numbered every way the grammar reads.", "probe_numbers.doc");
    described = describe_model_enum(&model, "probe_empty");
    CHECK(described != NULL && described->entry_count == 1 && described->entries[0].number == 0);
    describe_model_release(&model);
}

static void binds_an_enum_by_doc(void) {
    describe_model model;
    const char *paths[] = {WEIVEL_DESCRIBE_SERVICES_HEADER, FIXTURE("clauses.h")};
    describe_names names = describe_names_of_the_standard();
    static const expected_item line[] = {{"start", K(TEXT), MEMBER, NO, NULL, NULL, NULL},
                                         {"level", K(U32), MEMBER, NO, "probe_level", NULL, NULL},
                                         {"plain", K(U32), MEMBER, NO, NULL, NULL, NULL}};
    static const expected_item choice[] = {{"key", K(TEXT), MEMBER, NO, NULL, NULL, NULL},
                                           {"color", K(U32), MEMBER, NO, "probe_color", NULL, NULL}};
    static const expected_item paint_type[] = {{"size", K(U32), MEMBER, NO, NULL, NULL, NULL},
                                               {"color", K(U32), MEMBER, NO, "probe_color", NULL, NULL}};
    static const expected_item paint[] = {CONTEXT_PARAMETER,
                                          {"level", K(U32), K(INPUT), NO, "probe_level", NULL, NULL},
                                          {"color", K(U32), K(INPUT), NO, "probe_color", NULL, NULL},
                                          {"chosen", K(U32), K(ANSWER), NO, NULL, NULL, NULL},
                                          REFUSAL_PARAMETER};
    static const expected_item release[] = {CONTEXT_PARAMETER, RELEASED_REFUSAL};
    static const expected_operation operations[] = {
        OPERATION("paint", STATUS, NULL, paint),
        OPERATION("release_refusal", NOTHING, "weivel_standard_service_refusal", release)};
    if (read_all(&model, paths, COUNT_OF(paths), &names) != DESCRIBE_DONE) {
        CHECK(0);
        return;
    }
    check_members(&model, "probe_line", line, COUNT_OF(line));
    check_members(&model, "probe_choice", choice, COUNT_OF(choice));
    check_members(&model, "probe_paint", paint_type, COUNT_OF(paint_type));
    check_operations(&model, "probe_painter", operations, COUNT_OF(operations));
    describe_model_release(&model);
}

static void skips_what_declares_nothing(void) {
    describe_model model;
    const char *paths[] = {FIXTURE("skipped.h")};
    describe_names names = describe_names_of_the_standard();
    static const expected_item plain[] = {{"value", K(U32), MEMBER, NO, NULL, NULL, NULL},
                                          {"other", K(U64), MEMBER, NO, NULL, NULL, NULL}};
    if (read_all(&model, paths, COUNT_OF(paths), &names) != DESCRIBE_DONE) {
        CHECK(0);
        return;
    }
    CHECK(model.type_count == 1 && model.enum_count == 0 && model.handle_count == 0);
    check_members(&model, "probe_plain", plain, COUNT_OF(plain));
    check_shape(&model, "probe_plain", 0, NULL);
    CHECK(model.types[0].doc == NULL && model.types[0].members[0].doc == NULL && model.types[0].members[1].doc == NULL);
    describe_model_release(&model);
}

static void reads_restated_names_across_headers(void) {
    describe_model model;
    const char *paths[] = {FIXTURE("restated.h"), FIXTURE("restated_types.h")};
    describe_names names;
    static const expected_item ask[] = {
        CONTEXT_PARAMETER,
        {"question", K(POINTER), K(INPUT), K(VALUE), NULL, NULL, NULL},
        {"tree", K(VALUE), K(ANSWER), NO, NULL, NULL, "release_tree"},
        {"reply", K(POINTER), K(ANSWER), K(STRUCT), "probe_reply", NULL, "release_reply"},
        {"refusal", K(POINTER), K(REFUSAL), K(STRUCT), "probe_refusal", NULL, "release_refusal"}};
    static const expected_item release_tree[] = {CONTEXT_PARAMETER,
                                                 {"tree", K(VALUE), K(ANSWER), NO, NULL, NULL, NULL}};
    static const expected_item release_reply[] = {
        CONTEXT_PARAMETER, {"reply", K(POINTER), K(ANSWER), K(STRUCT), "probe_reply", NULL, NULL}};
    static const expected_item release_refusal[] = {
        CONTEXT_PARAMETER, {"refusal", K(POINTER), K(REFUSAL), K(STRUCT), "probe_refusal", NULL, NULL}};
    static const expected_operation operations[] = {
        OPERATION("ask", STATUS, NULL, ask), OPERATION("release_tree", NOTHING, "probe_tree", release_tree),
        OPERATION("release_reply", NOTHING, "probe_reply", release_reply),
        OPERATION("release_refusal", NOTHING, "probe_refusal", release_refusal)};
    static const char *const reached[] = {"probe_restated", "probe_refusal", "probe_reply"};
    names = describe_names_of_the_standard();
    names.status = "probe_status";
    names.refusal = "probe_refusal";
    names.value = "probe_value";
    names.tree = "probe_tree";
    if (read_all(&model, paths, COUNT_OF(paths), &names) != DESCRIBE_DONE) {
        CHECK(0);
        return;
    }
    check_operations(&model, "probe_restated", operations, COUNT_OF(operations));
    check_reach(&model, "probe_restated", reached, COUNT_OF(reached), NULL, 0);
    describe_model_release(&model);
}

static void counts_y_plurals_and_releases_only_what_owns_storage(void) {
    describe_model model;
    const char *paths[] = {FIXTURE("plurals_and_owners.h")};
    describe_names names = describe_names_of_the_standard();
    static const expected_item index[] = {
        {"size", K(U32), MEMBER, NO, NULL, NULL, NULL},
        {"entries", K(ARRAY), MEMBER, K(STRUCT), "probe_entry", "entry_count", NULL},
        {"entry_count", K(SIZE), MEMBER, NO, NULL, NULL, NULL},
        {"libraries", K(TEXTS), MEMBER, NO, NULL, "library_count", NULL},
        {"library_count", K(SIZE), MEMBER, NO, NULL, NULL, NULL},
        {"walk", K(HANDLE), MEMBER, NO, "probe_walk", NULL, "release_walk"}};
    static const expected_item tally[] = {CONTEXT_PARAMETER,
                                          {"index", K(POINTER), K(INPUT), K(STRUCT), "probe_index", NULL, NULL},
                                          {"tally", K(POINTER), K(ANSWER), K(STRUCT), "probe_tally", NULL, NULL}};
    static const expected_item count[] = {
        CONTEXT_PARAMETER, {"counted", K(POINTER), K(ANSWER), K(STRUCT), "probe_count", NULL, "release_count"}};
    static const expected_item nest[] = {
        CONTEXT_PARAMETER, {"outer", K(POINTER), K(ANSWER), K(STRUCT), "probe_outer", NULL, "release_outer"}};
    static const expected_item peek[] = {CONTEXT_PARAMETER,
                                         {"walk", K(HANDLE), K(INPUT), NO, "probe_walk", NULL, "release_walk"},
                                         {"seen", K(U64), K(ANSWER), NO, NULL, NULL, NULL}};
    static const expected_item measure[] = {CONTEXT_PARAMETER,
                                            {"entry_count", K(SIZE), K(ANSWER), NO, NULL, NULL, NULL}};
    static const expected_item release_count[] = {
        CONTEXT_PARAMETER, {"counted", K(POINTER), K(ANSWER), K(STRUCT), "probe_count", NULL, NULL}};
    static const expected_item release_outer[] = {
        CONTEXT_PARAMETER, {"outer", K(POINTER), K(ANSWER), K(STRUCT), "probe_outer", NULL, NULL}};
    static const expected_item release_walk[] = {
        CONTEXT_PARAMETER, {"walk", K(HANDLE), K(INPUT), NO, "probe_walk", NULL, "release_walk"}};
    static const expected_operation operations[] = {
        OPERATION("tally", STATUS, NULL, tally),
        OPERATION("count", STATUS, NULL, count),
        OPERATION("nest", STATUS, NULL, nest),
        OPERATION("peek", STATUS, NULL, peek),
        OPERATION("measure", STATUS, NULL, measure),
        OPERATION("release_count", NOTHING, "probe_count", release_count),
        OPERATION("release_outer", NOTHING, "probe_outer", release_outer),
        OPERATION("release_walk", NOTHING, "probe_walk", release_walk)};
    if (read_all(&model, paths, COUNT_OF(paths), &names) != DESCRIBE_DONE) {
        CHECK(0);
        return;
    }
    check_members(&model, "probe_index", index, COUNT_OF(index));
    check_operations(&model, "probe_indexer", operations, COUNT_OF(operations));
    describe_model_release(&model);
}

static void walks_what_an_answer_holds_once_through_cycles(void) {
    describe_model model;
    const char *paths[] = {FIXTURE("cycles.h")};
    describe_names names = describe_names_of_the_standard();
    static const expected_item ring[] = {
        CONTEXT_PARAMETER, {"ring", K(POINTER), K(ANSWER), K(STRUCT), "probe_ring_00", NULL, NULL}};
    static const expected_item far[] = {
        CONTEXT_PARAMETER, {"far", K(POINTER), K(ANSWER), K(STRUCT), "probe_far_00", NULL, "release_far"}};
    static const expected_item holder[] = {
        CONTEXT_PARAMETER,
        {"holder", K(POINTER), K(ANSWER), K(STRUCT), "probe_loop_holder", NULL, "release_loop_holder"}};
    static const expected_item back[] = {
        CONTEXT_PARAMETER, {"back", K(POINTER), K(ANSWER), K(STRUCT), "probe_loop_back", NULL, "release_loop_back"}};
    const describe_type *graph;
    if (read_all(&model, paths, COUNT_OF(paths), &names) != DESCRIBE_DONE) {
        CHECK(0);
        return;
    }
    graph = type_checked(&model, "probe_graph");
    if (graph != NULL && graph->operation_count == 7) {
        check_items("ring", graph->operations[0].parameters, graph->operations[0].parameter_count, ring,
                    COUNT_OF(ring));
        check_items("far", graph->operations[1].parameters, graph->operations[1].parameter_count, far, COUNT_OF(far));
        check_items("holder", graph->operations[2].parameters, graph->operations[2].parameter_count, holder,
                    COUNT_OF(holder));
        check_items("back", graph->operations[3].parameters, graph->operations[3].parameter_count, back,
                    COUNT_OF(back));
    } else {
        CHECK(0);
    }
    describe_model_release(&model);
}

/** A member spelled `const struct <tag> *` inside its own struct describes as its typedef name would. */
static void reads_a_struct_pointing_at_itself_by_its_tag(void) {
    describe_model model;
    const char *paths[] = {FIXTURE("self_reference.h")};
    describe_names names = describe_names_of_the_standard();
    static const expected_item spelled[] = {
        {"label", K(TEXT), MEMBER, NO, NULL, NULL, NULL},
        {"next", K(POINTER), MEMBER, K(STRUCT), "probe_spelled", NULL, NULL},
        {"content", K(ARRAY), MEMBER, K(STRUCT), "probe_spelled", "content_count", NULL},
        {"content_count", K(SIZE), MEMBER, NO, NULL, NULL, NULL}};
    static const expected_item named[] = {
        {"label", K(TEXT), MEMBER, NO, NULL, NULL, NULL},
        {"next", K(POINTER), MEMBER, K(STRUCT), "probe_named", NULL, NULL},
        {"content", K(ARRAY), MEMBER, K(STRUCT), "probe_named", "content_count", NULL},
        {"content_count", K(SIZE), MEMBER, NO, NULL, NULL, NULL}};
    static const expected_item copy[] = {
        CONTEXT_PARAMETER,
        {"spelled", K(POINTER), K(INPUT), K(STRUCT), "probe_spelled", NULL, NULL},
        {"named", K(POINTER), K(INPUT), K(STRUCT), "probe_named", NULL, NULL},
        {"tree", K(POINTER), K(ANSWER), K(STRUCT), "probe_tree", NULL, "release_tree"}};
    static const expected_item release_tree[] = {CONTEXT_PARAMETER,
                                                 {"tree", K(POINTER), K(ANSWER), K(STRUCT), "probe_tree", NULL, NULL}};
    static const expected_operation operations[] = {OPERATION("copy", STATUS, NULL, copy),
                                                    OPERATION("release_tree", NOTHING, "probe_tree", release_tree)};
    if (read_all(&model, paths, COUNT_OF(paths), &names) != DESCRIBE_DONE) {
        CHECK(0);
        return;
    }
    check_members(&model, "probe_spelled", spelled, COUNT_OF(spelled));
    check_members(&model, "probe_named", named, COUNT_OF(named));
    check_shape(&model, "probe_spelled", 0, NULL);
    check_operations(&model, "probe_trees", operations, COUNT_OF(operations));
    describe_model_release(&model);
}

static const expected_item REFUSAL_MEMBERS[] = {{"size", K(U32), MEMBER, NO, NULL, NULL, NULL},
                                                {"detail", K(TEXT), MEMBER, NO, NULL, NULL, NULL},
                                                {"fix", K(TEXT), MEMBER, NO, NULL, NULL, NULL},
                                                {"storage", K(STORAGE), MEMBER, NO, NULL, NULL, NULL}};

static const expected_item SETTING_VALUE[] = {
    {"kind", K(U32), MEMBER, NO, "weivel_standard_setting_kind", NULL, NULL},
    {"boolean", K(U32), MEMBER, NO, NULL, NULL, NULL},
    {"number", K(F64), MEMBER, NO, NULL, NULL, NULL},
    {"text", K(TEXT), MEMBER, NO, NULL, NULL, NULL},
    {"text_length", K(SIZE), MEMBER, NO, NULL, NULL, NULL}};

static const expected_item SETTING[] = {
    {"key", K(TEXT), MEMBER, NO, NULL, NULL, NULL},
    {"value", K(STRUCT), MEMBER, NO, "weivel_standard_setting_value", NULL, NULL}};

static const expected_item SETTINGS[] = {
    {"size", K(U32), MEMBER, NO, NULL, NULL, NULL},
    {"settings", K(ARRAY), MEMBER, K(STRUCT), "weivel_standard_setting", "count", NULL},
    {"count", K(SIZE), MEMBER, NO, NULL, NULL, NULL},
    {"storage", K(STORAGE), MEMBER, NO, NULL, NULL, NULL}};

static const expected_item SETTING_ANSWER[] = {
    {"size", K(U32), MEMBER, NO, NULL, NULL, NULL},
    {"value", K(STRUCT), MEMBER, NO, "weivel_standard_setting_value", NULL, NULL},
    {"storage", K(STORAGE), MEMBER, NO, NULL, NULL, NULL}};

static const expected_item SECRET[] = {{"size", K(U32), MEMBER, NO, NULL, NULL, NULL},
                                       {"bytes", K(BYTES), MEMBER, NO, NULL, "length", NULL},
                                       {"length", K(SIZE), MEMBER, NO, NULL, NULL, NULL},
                                       {"storage", K(STORAGE), MEMBER, NO, NULL, NULL, NULL}};

static const expected_item CONFIG_ALL[] = {
    CONTEXT_PARAMETER,
    {"settings", K(POINTER), K(ANSWER), K(STRUCT), "weivel_standard_settings", NULL, "release_settings"},
    REFUSAL_PARAMETER};
static const expected_item CONFIG_GET[] = {
    CONTEXT_PARAMETER,
    {"key", K(TEXT), K(INPUT), NO, NULL, NULL, NULL},
    {"answer", K(POINTER), K(ANSWER), K(STRUCT), "weivel_standard_setting_answer", NULL, "release_answer"},
    REFUSAL_PARAMETER};
static const expected_item CONFIG_SET[] = {
    CONTEXT_PARAMETER,
    {"key", K(TEXT), K(INPUT), NO, NULL, NULL, NULL},
    {"value", K(POINTER), K(INPUT), K(STRUCT), "weivel_standard_setting_value", NULL, NULL},
    REFUSAL_PARAMETER};
static const expected_item RELEASE_SETTINGS[] = {
    CONTEXT_PARAMETER, {"settings", K(POINTER), K(ANSWER), K(STRUCT), "weivel_standard_settings", NULL, NULL}};
static const expected_item RELEASE_SETTING_ANSWER[] = {
    CONTEXT_PARAMETER, {"answer", K(POINTER), K(ANSWER), K(STRUCT), "weivel_standard_setting_answer", NULL, NULL}};

static const expected_item SECRETS_REMOVE[] = {CONTEXT_PARAMETER, {"name", K(TEXT), K(INPUT), NO, NULL, NULL, NULL},
                                               REFUSAL_PARAMETER};
static const expected_item SECRETS_GET[] = {
    CONTEXT_PARAMETER,
    {"name", K(TEXT), K(INPUT), NO, NULL, NULL, NULL},
    {"secret", K(POINTER), K(ANSWER), K(STRUCT), "weivel_standard_secret", NULL, "release_secret"},
    REFUSAL_PARAMETER};
static const expected_item SECRETS_SET[] = {CONTEXT_PARAMETER,
                                            {"name", K(TEXT), K(INPUT), NO, NULL, NULL, NULL},
                                            {"bytes", K(BYTES), K(INPUT), NO, NULL, "length", NULL},
                                            {"length", K(SIZE), K(INPUT), NO, NULL, NULL, NULL},
                                            REFUSAL_PARAMETER};
static const expected_item RELEASE_SECRET[] = {
    CONTEXT_PARAMETER, {"secret", K(POINTER), K(ANSWER), K(STRUCT), "weivel_standard_secret", NULL, NULL}};

static const expected_item PATH[] = {CONTEXT_PARAMETER, {"directory", K(TEXT), K(ANSWER), NO, NULL, NULL, NULL}};
static const expected_item LOG_WRITE[] = {CONTEXT_PARAMETER,
                                          {"level", K(U32), K(INPUT), NO, "weivel_standard_log_level", NULL, NULL},
                                          {"message", K(TEXT), K(INPUT), NO, NULL, NULL, NULL},
                                          {"fields", K(POINTER), K(INPUT), K(VALUE), NULL, NULL, NULL},
                                          REFUSAL_PARAMETER};

static void reads_every_struct_and_definition_of_services_h(void) {
    describe_model model;
    const char *paths[] = {WEIVEL_DESCRIBE_SERVICES_HEADER};
    describe_names names = describe_names_of_the_standard();
    static const char *const types[] = {"weivel_standard_service_refusal", "weivel_standard_setting_value",
                                        "weivel_standard_setting",         "weivel_standard_settings",
                                        "weivel_standard_setting_answer",  "weivel_standard_config",
                                        "weivel_standard_secret",          "weivel_standard_secrets",
                                        "weivel_standard_paths",           "weivel_standard_log"};
    static const expected_operation config[] = {
        OPERATION("all", STATUS, NULL, CONFIG_ALL),
        OPERATION("get", STATUS, NULL, CONFIG_GET),
        OPERATION("set", STATUS, NULL, CONFIG_SET),
        OPERATION("release_settings", NOTHING, "weivel_standard_settings", RELEASE_SETTINGS),
        OPERATION("release_answer", NOTHING, "weivel_standard_setting_answer", RELEASE_SETTING_ANSWER),
        OPERATION("release_refusal", NOTHING, "weivel_standard_service_refusal", RELEASE_REFUSAL)};
    static const expected_operation secrets[] = {
        OPERATION("remove", STATUS, NULL, SECRETS_REMOVE),
        OPERATION("get", STATUS, NULL, SECRETS_GET),
        OPERATION("set", STATUS, NULL, SECRETS_SET),
        OPERATION("release_secret", NOTHING, "weivel_standard_secret", RELEASE_SECRET),
        OPERATION("release_refusal", NOTHING, "weivel_standard_service_refusal", RELEASE_REFUSAL)};
    static const expected_operation directories[] = {OPERATION("cache", STATUS, NULL, PATH),
                                                     OPERATION("config", STATUS, NULL, PATH),
                                                     OPERATION("plugin", STATUS, NULL, PATH)};
    static const expected_operation writer[] = {
        OPERATION("write", STATUS, NULL, LOG_WRITE),
        OPERATION("release_refusal", NOTHING, "weivel_standard_service_refusal", RELEASE_REFUSAL)};
    static const char *const levels[] = {"WEIVEL_STANDARD_LOG_TRACE", "WEIVEL_STANDARD_LOG_DEBUG",
                                         "WEIVEL_STANDARD_LOG_INFO", "WEIVEL_STANDARD_LOG_WARN",
                                         "WEIVEL_STANDARD_LOG_ERROR"};
    static const char *const kinds[] = {"WEIVEL_STANDARD_SETTING_NONE", "WEIVEL_STANDARD_SETTING_TEXT",
                                        "WEIVEL_STANDARD_SETTING_NUMBER", "WEIVEL_STANDARD_SETTING_BOOLEAN"};
    const describe_enum *kind;
    const describe_enum *level;
    size_t at;
    if (read_all(&model, paths, COUNT_OF(paths), &names) != DESCRIBE_DONE) {
        CHECK(0);
        return;
    }
    CHECK(model.type_count == COUNT_OF(types) && model.enum_count == 2 && model.handle_count == 0);
    for (at = 0; at < model.type_count && at < COUNT_OF(types); at++) {
        check_nullable(model.types[at].name, types[at], "services.h", "types");
    }
    check_members(&model, "weivel_standard_service_refusal", REFUSAL_MEMBERS, COUNT_OF(REFUSAL_MEMBERS));
    check_members(&model, "weivel_standard_setting_value", SETTING_VALUE, COUNT_OF(SETTING_VALUE));
    check_members(&model, "weivel_standard_setting", SETTING, COUNT_OF(SETTING));
    check_members(&model, "weivel_standard_settings", SETTINGS, COUNT_OF(SETTINGS));
    check_members(&model, "weivel_standard_setting_answer", SETTING_ANSWER, COUNT_OF(SETTING_ANSWER));
    check_members(&model, "weivel_standard_secret", SECRET, COUNT_OF(SECRET));
    check_members(&model, "weivel_standard_config", PROBE_DEFINITION, COUNT_OF(PROBE_DEFINITION));
    check_members(&model, "weivel_standard_secrets", PROBE_DEFINITION, COUNT_OF(PROBE_DEFINITION));
    check_members(&model, "weivel_standard_paths", PROBE_DEFINITION, COUNT_OF(PROBE_DEFINITION));
    check_members(&model, "weivel_standard_log", PROBE_DEFINITION, COUNT_OF(PROBE_DEFINITION));
    check_operations(&model, "weivel_standard_config", config, COUNT_OF(config));
    check_operations(&model, "weivel_standard_secrets", secrets, COUNT_OF(secrets));
    check_operations(&model, "weivel_standard_paths", directories, COUNT_OF(directories));
    check_operations(&model, "weivel_standard_log", writer, COUNT_OF(writer));
    check_shape(&model, "weivel_standard_service_refusal", 1, "storage");
    check_shape(&model, "weivel_standard_setting_value", 0, NULL);
    check_shape(&model, "weivel_standard_setting", 0, NULL);
    check_shape(&model, "weivel_standard_settings", 1, "storage");
    check_shape(&model, "weivel_standard_setting_answer", 1, "storage");
    check_shape(&model, "weivel_standard_config", 1, "release_refusal");
    check_shape(&model, "weivel_standard_secret", 1, "storage");
    check_shape(&model, "weivel_standard_secrets", 1, "release_refusal");
    check_shape(&model, "weivel_standard_paths", 1, "plugin");
    check_shape(&model, "weivel_standard_log", 1, "release_refusal");
    kind = describe_model_enum(&model, "weivel_standard_setting_kind");
    CHECK(kind != NULL && kind->entry_count == COUNT_OF(kinds));
    for (at = 0; kind != NULL && at < kind->entry_count && at < COUNT_OF(kinds); at++) {
        check_nullable(kind->entries[at].name, kinds[at], "weivel_standard_setting_kind", "name");
        check_number(kind->entries[at].number, (uint32_t)at, kinds[at], "number");
    }
    level = describe_model_enum(&model, "weivel_standard_log_level");
    CHECK(level != NULL && level->entry_count == COUNT_OF(levels));
    for (at = 0; level != NULL && at < level->entry_count && at < COUNT_OF(levels); at++) {
        check_nullable(level->entries[at].name, levels[at], "weivel_standard_log_level", "name");
        check_number(level->entries[at].number, (uint32_t)at + 1, levels[at], "number");
    }
    describe_model_release(&model);
}

static void reaches_what_each_service_of_services_h_uses(void) {
    describe_model model;
    const char *paths[] = {WEIVEL_DESCRIBE_SERVICES_HEADER};
    describe_names names = describe_names_of_the_standard();
    static const char *const config[] = {"weivel_standard_service_refusal", "weivel_standard_setting_value",
                                         "weivel_standard_setting", "weivel_standard_settings",
                                         "weivel_standard_setting_answer", "weivel_standard_config"};
    static const char *const kinds[] = {"weivel_standard_setting_kind"};
    static const char *const secrets[] = {"weivel_standard_service_refusal", "weivel_standard_secret",
                                          "weivel_standard_secrets"};
    static const char *const directories[] = {"weivel_standard_paths"};
    static const char *const writer[] = {"weivel_standard_service_refusal", "weivel_standard_log"};
    static const char *const levels[] = {"weivel_standard_log_level"};
    if (read_all(&model, paths, COUNT_OF(paths), &names) != DESCRIBE_DONE) {
        CHECK(0);
        return;
    }
    check_reach(&model, "weivel_standard_config", config, COUNT_OF(config), kinds, COUNT_OF(kinds));
    check_reach(&model, "weivel_standard_secrets", secrets, COUNT_OF(secrets), NULL, 0);
    check_reach(&model, "weivel_standard_paths", directories, COUNT_OF(directories), NULL, 0);
    check_reach(&model, "weivel_standard_log", writer, COUNT_OF(writer), levels, COUNT_OF(levels));
    describe_model_release(&model);
}

typedef struct refused_case {
    const char *path;
    uint32_t line;
    /** A phrase the message holds. */
    const char *says;
} refused_case;

#define GRAMMAR "is not in weivel's description grammar"

static const refused_case REFUSED_CASES[] = {
    {REFUSED("union.h"), 3, "a union " GRAMMAR},
    {REFUSED("bit_field.h"), 2, "the bit field `low` " GRAMMAR},
    {REFUSED("fixed_array.h"), 2, "the fixed array `values` " GRAMMAR},
    {REFUSED("function_shape.h"), 5, "a function pointer of another shape " GRAMMAR},
    {REFUSED("unknown_type.h"), 2, "`probe_mystery`, the type of the member `thing`, which no scanned header declares, "
                                   GRAMMAR},
    {REFUSED("keyword_type.h"), 2, "`unsigned` in a type " GRAMMAR},
    {REFUSED("struct_other_tag.h"), 2, "a type spelled with `struct` inside a struct " GRAMMAR ": a struct names "
                                       "each type by its typedef name, and spells `struct` only in a member "
                                       "pointing at its own struct, as `const struct probe_holder *`"},
    {REFUSED("struct_held_by_itself.h"), 3, "`struct probe_block` held by value inside itself, which leaves it no "
                                            "finite size, " GRAMMAR ": point at it as `const struct probe_block *`"},
    {REFUSED("struct_own_tag_parameter.h"), 5, "a type spelled with `struct` inside a struct " GRAMMAR},
    {REFUSED("enum_by_value.h"), 4, "holds the enum `probe_mode` as its own type"},
    {REFUSED("mutable_text.h"), 2, "the member `name` of type `char *` " GRAMMAR},
    {REFUSED("mutable_struct_member.h"), 6, "the member `item` of type `probe_item *` " GRAMMAR},
    {REFUSED("context_outside.h"), 2, "`void *context` outside a definition " GRAMMAR},
    {REFUSED("other_void.h"), 2, "the member `data` of type `void *` " GRAMMAR},
    {REFUSED("no_first_shape.h"), 2, "`probe_sized` begins with `uint32_t size` and its doc names no first shape"},
    {REFUSED("unsized_shape.h"), 2, "`probe_flat` names a first shape and does not begin with `uint32_t size`"},
    {REFUSED("shape_undeclared.h"), 2, "ends at `missing`, which it does not declare"},
    {REFUSED("definition_extra.h"), 5, "the member `extra` of the definition `probe_service`, beside its size and "
                                       "context, " GRAMMAR},
    {REFUSED("definition_unsized.h"), 1, "the definition `probe_service`, not beginning `uint32_t size; void "
                                         "*context;`, " GRAMMAR},
    {REFUSED("member_after_operations.h"), 6, "the member `extra` after the definition's function pointers " GRAMMAR},
    {REFUSED("returns_other.h"), 5, "the operation `run` returning `uint32_t`, neither `weivel_status` nor void, "
                                    GRAMMAR},
    {REFUSED("no_context.h"), 5, "the operation `run`, whose first parameter is not `void *context`, " GRAMMAR},
    {REFUSED("unnamed_parameter.h"), 5, "a parameter without a name " GRAMMAR},
    {REFUSED("struct_by_value_parameter.h"), 9, "the parameter `item` of type `probe_item` " GRAMMAR},
    {REFUSED("refusal_not_last.h"), 13, "the refusal `refusal` before the last parameter " GRAMMAR},
    {REFUSED("refusal_undeclared.h"), 5, "`weivel_standard_service_refusal`, the type of the parameter `refusal`, "
                                         "which no scanned header declares"},
    {REFUSED("answer_unreleased.h"), 19, "`answer` of `run` is a `probe_answer`, which holds storage, and no "
                                         "release of `probe_service` frees it"},
    {REFUSED("nested_storage_unreleased.h"), 16, "`outer` of `nest` is a `probe_outer`, which holds storage, and no "
                                                 "release of `probe_service` frees it"},
    {REFUSED("cycle_storage_unreleased.h"), 15, "`back` of `back` is a `probe_loop_back`, which holds storage, and "
                                                "no release of `probe_graph` frees it"},
    {REFUSED("count_counting_nothing.h"), 7, "`child_count` is named as a count and counts nothing beside it"},
    {REFUSED("length_counting_nothing.h"), 3, "`label_length` is named as a count and counts nothing beside it"},
    {REFUSED("two_releases.h"), 21, "the release `release_two` frees `probe_answer`, as `release_one` already does"},
    {REFUSED("release_shape.h"), 19, "the release `release_answer`, taking other than its context and one pointer, "
                                     GRAMMAR},
    {REFUSED("handle_unreleased.h"), 4, "`walk` holds the handle `probe_walk`, which no release of any definition "
                                        "frees"},
    {REFUSED("handle_two_names.h"), 22, "the release `free_walk` frees the handle `probe_walk`, which "
                                        "`release_walk` frees elsewhere"},
    {REFUSED("texts_uncounted.h"), 2, "`tags` has no size_t beside it counting it, named `tags_count`, `tag_count` or "
                                      "`count`"},
    {REFUSED("array_uncounted.h"), 2, "`numbers` has no size_t beside it counting it"},
    {REFUSED("bytes_uncounted.h"), 2, "`bytes` has no size_t beside it counting it, named `bytes_length` or "
                                      "`length`"},
    {REFUSED("count_not_size.h"), 7, "`item_count` would count `items` by its name and is not a size_t"},
    {REFUSED("two_counts.h"), 6, "`items` could be counted by both `item_count` and `count`"},
    {REFUSED("counted_twice.h"), 7, "`count` counts both `tags` and `items`"},
    {REFUSED("own_doc_two_enums.h"), 6, "the doc of `mode` names both `probe_mode` and `probe_level`"},
    {REFUSED("clause_two_enums.h"), 5, "a clause of the doc of `probe_run` names both `probe_mode` and "
                                       "`probe_level`"},
    {REFUSED("clause_ambiguous.h"), 4, "the doc of `probe_run` names `probe_mode` without saying which uint32_t "
                                       "holds it"},
    {REFUSED("clause_two_members.h"), 4, "the doc of `probe_run` names `probe_mode` without saying which uint32_t "
                                         "holds it"},
    {REFUSED("bound_twice.h"), 7, "`mode` is said to hold both `probe_mode` and `probe_level`"},
    {REFUSED("anonymous_struct.h"), 1, "an anonymous struct " GRAMMAR},
    {REFUSED("anonymous_enum.h"), 1, "an anonymous enum " GRAMMAR},
    {REFUSED("typedef_mismatch.h"), 3, "a typedef naming `probe_b` other than its tag `probe_a` " GRAMMAR},
    {REFUSED("typedef_other.h"), 1, "a typedef that is not of a struct or an enum " GRAMMAR},
    {REFUSED("top_level.h"), 3, "`static` at the top level " GRAMMAR},
    {REFUSED("function_with_body.h"), 1, "`uint32_t` at the top level, a function with a body or of another shape, "
                                         GRAMMAR},
    {REFUSED("open_comment.h"), 3, "a comment that never ends"},
    {REFUSED("open_extern.h"), 8, "an `extern \"C\" {` never closed " GRAMMAR},
    {REFUSED("repeated_struct.h"), 5, "the struct `probe_plain` repeats a name declared before it"},
    {REFUSED("repeated_member.h"), 3, "the member `value` repeats a name declared before it"},
    {REFUSED("defined_after_forward.h"), 3, "the struct `probe_plain` repeats a name declared before it"},
    {REFUSED("negative_entry.h"), 2, "`-` as an enum's number " GRAMMAR},
    {REFUSED("large_entry.h"), 2, "`4294967296` as an enum's number, past 2^32 - 1, " GRAMMAR},
    {REFUSED("implicit_overflow.h"), 3, "`PROBE_PAST`, numbered past 2^32 - 1, " GRAMMAR},
    {REFUSED("repeated_entry.h"), 4, "the entry `PROBE_FAST` repeats a name declared before it"},
    {REFUSED("blocking_release.h"), 13, "the release `release_answer` is tagged `@blocking`"},
    {REFUSED("definition_input.h"), 12, "the parameter `counter` points at the definition `probe_counter` as an "
                                        "input"},
    {REFUSED("missing.h"), 0, "the header cannot be read"}};

static void refuses_each_form_outside_the_grammar_at_its_line(void) {
    describe_names names = describe_names_of_the_standard();
    size_t at;
    for (at = 0; at < COUNT_OF(REFUSED_CASES); at++) {
        const refused_case *refused = &REFUSED_CASES[at];
        const char *paths[1];
        describe_model model;
        describe_refusal refusal;
        describe_outcome outcome;
        paths[0] = refused->path;
        outcome = describe_read(&model, paths, 1, &names, &refusal);
        if (outcome != DESCRIBE_REFUSED) {
            fprintf(stderr, "%s: not refused (%d)\n", refused->path, (int)outcome);
            check_failures++;
            if (outcome == DESCRIBE_DONE) {
                describe_model_release(&model);
            }
            continue;
        }
        CHECK(model.type_count == 0 && model.types == NULL && model.files == NULL);
        check_nullable(refusal.file, refused->path, refused->path, "file");
        check_number(refusal.line, refused->line, refused->path, "line");
        if (refusal.message == NULL || strstr(refusal.message, refused->says) == NULL) {
            fprintf(stderr, "%s: expected a message holding \"%s\", got \"%s\"\n", refused->path, refused->says,
                    refusal.message == NULL ? "(null)" : refusal.message);
            check_failures++;
        }
        describe_refusal_release(&refusal);
    }
}

static void refuses_a_reach_its_definition_cannot_release(void) {
    describe_model model;
    describe_reach reach;
    describe_refusal refusal;
    const char *paths[] = {REFUSED("reach_handle.h")};
    describe_names names = describe_names_of_the_standard();
    static const char *const walker[] = {"probe_walker"};
    if (read_all(&model, paths, COUNT_OF(paths), &names) != DESCRIBE_DONE) {
        CHECK(0);
        return;
    }
    check_reach(&model, "probe_walker", walker, COUNT_OF(walker), NULL, 0);
    memset(&refusal, 0, sizeof refusal);
    CHECK(describe_reach_of(&model, "probe_keeper", &reach, &refusal) == DESCRIBE_REFUSED);
    CHECK(reach.types == NULL && reach.type_count == 0);
    check_nullable(refusal.file, REFUSED("reach_handle.h"), "reach", "file");
    check_number(refusal.line, 14, "reach", "line");
    CHECK(refusal.message != NULL &&
          strstr(refusal.message, "`walk` holds the handle `probe_walk`, which `probe_keeper` reaches but holds no "
                                  "release `release_walk` of") != NULL);
    describe_reach_release(&reach);
    describe_refusal_release(&refusal);
    CHECK(describe_reach_of(&model, "probe_holder", &reach, &refusal) == DESCRIBE_REFUSED);
    CHECK(refusal.file == NULL && refusal.line == 0 && refusal.message != NULL &&
          strstr(refusal.message, "`probe_holder` is not a definition") != NULL);
    describe_reach_release(&reach);
    describe_refusal_release(&refusal);
    CHECK(describe_reach_of(&model, "probe_missing", &reach, &refusal) == DESCRIBE_REFUSED);
    describe_reach_release(&reach);
    describe_refusal_release(&refusal);
    describe_model_release(&model);
}

static const char *const NESTED_ELSEWHERE[] = {"probe_remote"};

static describe_names names_with_a_remote(void) {
    describe_names names = describe_names_of_the_standard();
    names.elsewhere = NESTED_ELSEWHERE;
    names.elsewhere_count = COUNT_OF(NESTED_ELSEWHERE);
    return names;
}

static void check_blocking(const describe_model *model, const char *definition, const char *operation,
                           int blocking) {
    const describe_type *type = describe_model_type(model, definition);
    const describe_operation *found = type == NULL ? NULL : describe_type_operation(type, operation);
    CHECK(found != NULL);
    if (found != NULL) {
        check_number((uint32_t)found->blocking, (uint32_t)blocking, operation, "blocking");
    }
}

static void reads_a_definition_held_and_answered_and_an_operation_that_blocks(void) {
    describe_model model;
    describe_reach reach;
    describe_refusal refusal;
    const char *paths[] = {FIXTURE("nested.h")};
    describe_names names = names_with_a_remote();
    static const expected_item lease[] = {{"size", K(U32), MEMBER, NO, NULL, NULL, NULL},
                                          {"counter", K(DEFINITION), MEMBER, NO, "probe_counter", NULL, NULL},
                                          {"remote", K(DEFINITION), MEMBER, NO, "probe_remote", NULL, NULL}};
    static const expected_item lend[] = {CONTEXT_PARAMETER,
                                         {"lease", K(POINTER), K(ANSWER), K(STRUCT), "probe_lease", NULL, NULL}};
    static const expected_item borrow[] = {CONTEXT_PARAMETER,
                                           {"counter", K(DEFINITION), K(ANSWER), NO, "probe_counter", NULL, NULL}};
    static const expected_operation operations[] = {OPERATION("lend", STATUS, NULL, lend),
                                                    OPERATION("borrow", STATUS, NULL, borrow)};
    static const char *const types[] = {"probe_lease", "probe_lender"};
    if (read_all(&model, paths, COUNT_OF(paths), &names) != DESCRIBE_DONE) {
        CHECK(0);
        return;
    }
    check_members(&model, "probe_lease", lease, COUNT_OF(lease));
    check_operations(&model, "probe_lender", operations, COUNT_OF(operations));
    check_blocking(&model, "probe_lender", "lend", 1);
    check_blocking(&model, "probe_lender", "borrow", 0);
    check_blocking(&model, "probe_counter", "add", 0);
    check_reach(&model, "probe_lender", types, COUNT_OF(types), NULL, 0);
    CHECK(describe_reach_of(&model, "probe_lender", &reach, &refusal) == DESCRIBE_DONE);
    CHECK(reach.definition_count == 2);
    if (reach.definition_count == 2) {
        check_nullable(reach.definitions[0], "probe_counter", "reach", "definitions[0]");
        check_nullable(reach.definitions[1], "probe_remote", "reach", "definitions[1]");
    }
    describe_reach_release(&reach);
    describe_model_release(&model);
}

/** A definition the headers declare and that is named as described elsewhere is read from them and still elsewhere. */
static void reads_a_definition_both_scanned_and_described_elsewhere(void) {
    describe_model model;
    const char *paths[] = {FIXTURE("nested_remote.h"), FIXTURE("nested.h")};
    static const char *const elsewhere[] = {"probe_remote", "probe_counter"};
    static const expected_item lease[] = {{"size", K(U32), MEMBER, NO, NULL, NULL, NULL},
                                          {"counter", K(DEFINITION), MEMBER, NO, "probe_counter", NULL, NULL},
                                          {"remote", K(DEFINITION), MEMBER, NO, "probe_remote", NULL, NULL}};
    const describe_type *remote;
    describe_names names = describe_names_of_the_standard();
    names.elsewhere = elsewhere;
    names.elsewhere_count = COUNT_OF(elsewhere);
    if (read_all(&model, paths, COUNT_OF(paths), &names) != DESCRIBE_DONE) {
        CHECK(0);
        return;
    }
    check_members(&model, "probe_lease", lease, COUNT_OF(lease));
    remote = describe_model_type(&model, "probe_remote");
    CHECK(remote != NULL && remote->definition && remote->elsewhere);
    CHECK(describe_model_type(&model, "probe_counter")->elsewhere);
    CHECK(!describe_model_type(&model, "probe_lender")->elsewhere);
    CHECK(describe_model_declares_description(&model, "probe_remote"));
    CHECK(!describe_model_declares_description(&model, "probe_counter"));
    CHECK(!describe_model_declares_description(&model, "probe_remote_description"));
    describe_model_release(&model);
}

static void refuses_a_struct_named_as_described_elsewhere(void) {
    describe_model model;
    describe_refusal refusal;
    const char *paths[] = {FIXTURE("nested.h")};
    static const char *const elsewhere[] = {"probe_remote", "probe_lease"};
    describe_names names = describe_names_of_the_standard();
    names.elsewhere = elsewhere;
    names.elsewhere_count = COUNT_OF(elsewhere);
    CHECK(describe_read(&model, paths, COUNT_OF(paths), &names, &refusal) == DESCRIBE_REFUSED);
    check_nullable(refusal.file, FIXTURE("nested.h"), "a struct named elsewhere", "file");
    check_number(refusal.line, 32, "a struct named elsewhere", "line");
    CHECK(refusal.message != NULL &&
          strstr(refusal.message, "`probe_lease` is named as described elsewhere and is declared here as a struct that "
                                  "is no definition") != NULL);
    describe_refusal_release(&refusal);
}

static void reads_a_header_in_memory(void) {
    describe_model model;
    describe_refusal refusal;
    const char *paths[] = {"in-memory.h"};
    const char *texts[] = {"typedef struct probe_pair {\n    uint64_t left;\n    uint64_t right;\n} probe_pair;\n"};
    size_t lengths[1];
    describe_names names = describe_names_of_the_standard();
    lengths[0] = strlen(texts[0]);
    CHECK(describe_read_texts(&model, paths, texts, lengths, 1, &names, &refusal) == DESCRIBE_DONE);
    CHECK(model.type_count == 1 && model.types[0].member_count == 2);
    check_where(model.types[0].members[1].where, "in-memory.h", 3, "probe_pair.right");
    describe_model_release(&model);
}

static void reads_a_header_with_crlf_line_ends(void) {
    describe_model model;
    describe_refusal refusal;
    const char *paths[] = {"crlf.h"};
    const char *texts[] = {"#define PROBE_JOIN(a, b) \\\r\n    a##b\r\n"
                           "/**\r\n * A pair. First shape\r\n * ends at `right`.\r\n */\r\n"
                           "typedef struct probe_pair {\r\n    uint32_t size;\r\n    uint64_t right;\r\n"
                           "} probe_pair;\r\n"};
    size_t lengths[1];
    describe_names names = describe_names_of_the_standard();
    lengths[0] = strlen(texts[0]);
    CHECK(describe_read_texts(&model, paths, texts, lengths, 1, &names, &refusal) == DESCRIBE_DONE);
    CHECK(model.type_count == 1);
    check_shape(&model, "probe_pair", 1, "right");
    check_text(model.type_count == 1 ? model.types[0].doc : NULL, "A pair. First shape ends at `right`.",
               "probe_pair.doc");
    check_where(model.types[0].members[1].where, "crlf.h", 9, "probe_pair.right");
    describe_model_release(&model);
}

int main(void) {
    reads_a_member_of_every_kind();
    reads_an_operation_of_every_form();
    keeps_each_parameter_doc();
    records_where_each_item_is_declared();
    reaches_what_a_definition_uses();
    numbers_enum_entries();
    binds_an_enum_by_doc();
    skips_what_declares_nothing();
    reads_restated_names_across_headers();
    counts_y_plurals_and_releases_only_what_owns_storage();
    walks_what_an_answer_holds_once_through_cycles();
    reads_a_struct_pointing_at_itself_by_its_tag();
    reads_every_struct_and_definition_of_services_h();
    reaches_what_each_service_of_services_h_uses();
    refuses_each_form_outside_the_grammar_at_its_line();
    refuses_a_reach_its_definition_cannot_release();
    reads_a_definition_held_and_answered_and_an_operation_that_blocks();
    reads_a_definition_both_scanned_and_described_elsewhere();
    refuses_a_struct_named_as_described_elsewhere();
    reads_a_header_in_memory();
    reads_a_header_with_crlf_line_ends();
    return check_summary("describe_test");
}
