#include "check.h"
#include "description_fixture.h"

#define INVALID WEIVEL_STANDARD_REFUSAL_INVALID_VALUE
#define MISSING WEIVEL_STANDARD_REFUSAL_MISSING_FIELD
#define DUPLICATE WEIVEL_STANDARD_REFUSAL_DUPLICATE

typedef struct newer_description {
    weivel_standard_description description;
    uint64_t appended_later;
} newer_description;

typedef struct refused_case {
    const char *name;
    void (*breaks)(probe_description *probe);
    uint32_t code;
    const char *path;
    /** A phrase the refusal's detail holds, so two faults refused at one path are told apart. */
    const char *says;
} refused_case;

static void accepts_a_description_of_every_kind(void) {
    probe_description probe;
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    probe_describe(&probe);
    CHECK(weivel_standard_description_check(&probe.description, &refusal) == WEIVEL_STATUS_OK);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_NONE && refusal.storage == NULL);
    CHECK(weivel_standard_description_check(&probe.description, NULL) == WEIVEL_STATUS_OK);
}

static void reads_a_newer_description_up_to_what_it_knows(void) {
    probe_description probe;
    newer_description newer;
    probe_describe(&probe);
    newer.description = probe.description;
    newer.description.size = (uint32_t)sizeof newer;
    newer.appended_later = UINT64_MAX;
    CHECK(weivel_standard_description_check(&newer.description, NULL) == WEIVEL_STATUS_OK);
}

/** Makes `member` a STRUCT of `type` filling a struct of `size` bytes, as the only member a struct holds. */
static void fill_with(weivel_standard_described_member *member, const char *type, size_t size) {
    member->offset = 0;
    member->size = size;
    member->kind = WEIVEL_STANDARD_DESCRIBED_STRUCT;
    member->element = WEIVEL_STANDARD_DESCRIBED_NONE;
    member->type = type;
    member->count = NULL;
    member->release = NULL;
}

/** Makes probe_item hold only a probe_loan by value, every struct holding an item resized to match. */
static void fills_the_item_with_a_loan(probe_description *probe) {
    size_t size = probe->types[PROBE_LOAN_TYPE].size;
    fill_with(&probe->item[0], "probe_loan", size);
    probe->types[PROBE_ITEM_TYPE].size = size;
    probe->types[PROBE_ITEM_TYPE].first_shape = size;
    probe->types[PROBE_ITEM_TYPE].member_count = 1;
    probe->input[9].size = size;
}

/**
 * A struct pointing at itself, through a POINTER and an ARRAY, and a cycle through a struct held by value and a
 * pointer back, are all bound as any other: only a struct holding itself by value has no size.
 */
static void accepts_a_struct_reaching_itself_through_a_pointer(void) {
    probe_description probe;
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    probe_describe(&probe);
    probe.input[10].type = "probe_input";
    probe.input[11].type = "probe_input";
    probe.item[0].kind = WEIVEL_STANDARD_DESCRIBED_POINTER;
    probe.item[0].element = WEIVEL_STANDARD_DESCRIBED_STRUCT;
    probe.item[0].type = "probe_input";
    CHECK(weivel_standard_description_check(&probe.description, &refusal) == WEIVEL_STATUS_OK);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_NONE && refusal.storage == NULL);
}

/** A struct filled by one it holds by value is sound while that one never holds it back. */
static void accepts_a_struct_filled_by_another_holding_it_nowhere(void) {
    probe_description probe;
    probe_describe(&probe);
    fills_the_item_with_a_loan(&probe);
    CHECK(weivel_standard_description_check(&probe.description, NULL) == WEIVEL_STATUS_OK);
}

static void holds_itself_by_value(probe_description *probe) {
    fill_with(&probe->item[0], "probe_item", probe->types[PROBE_ITEM_TYPE].size);
    probe->types[PROBE_ITEM_TYPE].member_count = 1;
}

static void holds_by_value_one_holding_it(probe_description *probe) {
    fills_the_item_with_a_loan(probe);
    fill_with(&probe->loan[0], "probe_item", probe->types[PROBE_LOAN_TYPE].size);
}

#define HELD_CHAIN_LENGTH 64

/** Structs past the probe's own, each of one size and filled by a member naming the next. */
typedef struct chain {
    char names[HELD_CHAIN_LENGTH][16];
    weivel_standard_described_member members[HELD_CHAIN_LENGTH][2];
    weivel_standard_described_type types[PROBE_TYPE_COUNT + HELD_CHAIN_LENGTH];
} chain;

/**
 * Appends `length` structs to the probe's types, each filled by `fillers` members naming the next; the last names
 * none, or the first when `closes`, which makes a cycle of all `length`.
 */
static void append_chain(probe_description *probe, chain *links, size_t length, size_t fillers, int closes) {
    size_t at;
    size_t filler;
    memcpy(links->types, probe->types, sizeof probe->types);
    for (at = 0; at < length; at++) {
        snprintf(links->names[at], sizeof links->names[at], "probe_chain_%02u", (unsigned)at);
    }
    for (at = 0; at < length; at++) {
        weivel_standard_described_type *type = &links->types[PROBE_TYPE_COUNT + at];
        size_t next = at + 1 < length ? at + 1 : 0;
        int names_next = at + 1 < length || closes;
        for (filler = 0; filler < fillers; filler++) {
            fill_with(&links->members[at][filler], links->names[next], sizeof(uint64_t));
            links->members[at][filler].name = filler == 0 ? "first" : "second";
        }
        type->name = links->names[at];
        type->size = sizeof(uint64_t);
        type->first_shape = sizeof(uint64_t);
        type->members = links->members[at];
        type->member_count = names_next ? fillers : 0;
    }
    probe->description.types = links->types;
    probe->description.type_count = PROBE_TYPE_COUNT + length;
}

static void check_chain(size_t length, size_t fillers, int closes, const char *path, const char *says) {
    static chain links;
    probe_description probe;
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    weivel_status status;
    probe_describe(&probe);
    append_chain(&probe, &links, length, fillers, closes);
    status = weivel_standard_description_check(&probe.description, &refusal);
    if (path == NULL) {
        CHECK(status == WEIVEL_STATUS_OK);
    } else {
        CHECK(status == WEIVEL_STATUS_MALFORMED);
        check_text(refusal.path, path, says);
        CHECK(refusal.detail != NULL && strstr(refusal.detail, says) != NULL);
    }
    weivel_standard_refusal_release(&refusal);
}

/**
 * A long chain of structs each filled by the next is sound, and closing it at any length is refused at its first
 * struct. Structs each filled by two members naming the next, which a walk following every filler takes 2^64 steps
 * through, are refused for the second filler before any walk.
 */
static void checks_chains_of_structs_held_by_value(void) {
    check_chain(HELD_CHAIN_LENGTH, 1, 0, NULL, NULL);
    check_chain(3, 1, 1, "types[6].members[0].type", "names a struct that is, or holds by value, the struct holding it");
    check_chain(HELD_CHAIN_LENGTH, 1, 1, "types[6].members[0].type",
                "names a struct that is, or holds by value, the struct holding it");
    check_chain(HELD_CHAIN_LENGTH, 2, 0, "types[6].members[1].offset", "lays a second STRUCT member over the whole");
}

static void names_no_struct(probe_description *probe) {
    probe->input[9].type = "probe_thing";
}

static void points_at_no_struct(probe_description *probe) {
    probe->run[1].type = "probe_thing";
}

static void names_no_enum(probe_description *probe) {
    probe->input[1].type = "probe_speed";
}

static void defines_no_struct(probe_description *probe) {
    probe->description.definition = "probe_thing";
}

static void puts_a_member_outside(probe_description *probe) {
    probe->item[1].offset = sizeof(probe_item);
}

static void sizes_a_member_wrongly(probe_description *probe) {
    probe->input[1].size = sizeof(uint64_t);
}

static void counts_with_nothing(probe_description *probe) {
    probe->input[11].count = NULL;
}

static void counts_with_a_missing_member(probe_description *probe) {
    probe->input[11].count = "probe_count";
}

static void counts_with_a_member_of_another_struct(probe_description *probe) {
    probe->input[5].count = "weight";
}

static void counts_with_a_member_not_a_size(probe_description *probe) {
    probe->input[11].count = "mode";
}

static void counts_bytes_with_a_missing_parameter(probe_description *probe) {
    probe->get[2].count = "size";
}

static void counts_bytes_with_a_parameter_not_a_size(probe_description *probe) {
    probe->get[2].count = "key";
}

static void puts_an_operation_outside(probe_description *probe) {
    probe->operations[PROBE_RELEASE_REFUSAL].offset = sizeof(probe_definition);
}

static void shapes_past_the_struct(probe_description *probe) {
    probe->types[PROBE_INPUT_TYPE].first_shape = sizeof(probe_input) + 1;
}

static void gives_no_kind(probe_description *probe) {
    probe->input[2].kind = WEIVEL_STANDARD_DESCRIBED_NONE;
}

static void gives_a_kind_past_the_last(probe_description *probe) {
    probe->answer[2].kind = WEIVEL_STANDARD_DESCRIBED_LAST + 1;
}

static void gives_a_parameter_a_kind_past_the_last(probe_description *probe) {
    probe->get[1].kind = WEIVEL_STANDARD_DESCRIBED_LAST + 1;
}

static void points_at_a_scalar(probe_description *probe) {
    probe->input[10].element = WEIVEL_STANDARD_DESCRIBED_U32;
}

static void holds_arrays_in_an_array(probe_description *probe) {
    probe->input[13].element = WEIVEL_STANDARD_DESCRIBED_ARRAY;
}

static void gives_no_role(probe_description *probe) {
    probe->run[2].role = WEIVEL_STANDARD_DESCRIBED_NO_ROLE;
}

static void gives_a_role_past_the_last(probe_description *probe) {
    probe->run[2].role = WEIVEL_STANDARD_DESCRIBED_ROLE_LAST + 1;
}

static void gives_no_return(probe_description *probe) {
    probe->operations[PROBE_GET].returns = WEIVEL_STANDARD_DESCRIBED_NO_RETURN;
}

static void gives_a_return_past_the_last(probe_description *probe) {
    probe->operations[PROBE_GET].returns = WEIVEL_STANDARD_DESCRIBED_RETURN_LAST + 1;
}

static void releases_through_nothing(probe_description *probe) {
    probe->run[2].release = "release_everything";
}

static void releases_a_handle_through_nothing(probe_description *probe) {
    probe->answer[1].release = "release_everything";
}

static void leaves_a_handle_unreleased(probe_description *probe) {
    probe->answer[1].release = NULL;
}

static void leaves_a_handle_untyped(probe_description *probe) {
    probe->answer[1].type = NULL;
}

static void leaves_a_struct_untyped(probe_description *probe) {
    probe->input[9].type = NULL;
}

static void names_no_definition(probe_description *probe) {
    probe->description.definition = NULL;
}

static void leaves_a_type_unnamed(probe_description *probe) {
    probe->types[PROBE_ANSWER_TYPE].name = NULL;
}

static void leaves_a_member_unnamed(probe_description *probe) {
    probe->refusal[3].name = NULL;
}

static void leaves_an_enum_unnamed(probe_description *probe) {
    probe->enums[0].name = NULL;
}

static void leaves_an_enum_entry_unnamed(probe_description *probe) {
    probe->modes[1].name = NULL;
}

static void leaves_an_operation_unnamed(probe_description *probe) {
    probe->operations[PROBE_READ].name = NULL;
}

static void leaves_a_parameter_unnamed(probe_description *probe) {
    probe->release_tree[1].name = NULL;
}

static void counts_types_it_does_not_hold(probe_description *probe) {
    probe->description.types = NULL;
}

static void counts_members_it_does_not_hold(probe_description *probe) {
    probe->types[PROBE_ITEM_TYPE].members = NULL;
}

static void counts_parameters_it_does_not_hold(probe_description *probe) {
    probe->operations[PROBE_RUN].parameters = NULL;
}

static void holds_texts_in_an_array(probe_description *probe) {
    probe->input[13].element = WEIVEL_STANDARD_DESCRIBED_TEXT;
}

static void takes_a_value_by_value(probe_description *probe) {
    probe->read[1].kind = WEIVEL_STANDARD_DESCRIBED_VALUE;
    probe->read[1].element = WEIVEL_STANDARD_DESCRIBED_NONE;
}

static void names_a_type_twice(probe_description *probe) {
    probe->types[PROBE_DEFINITION_TYPE].name = "probe_item";
}

static void names_an_enum_twice(probe_description *probe) {
    static weivel_standard_described_enum twice[2];
    twice[0] = probe->enums[0];
    twice[1] = probe->enums[0];
    probe->description.enums = twice;
    probe->description.enum_count = 2;
}

static void names_an_operation_twice(probe_description *probe) {
    probe->operations[PROBE_RELEASE_TREE].name = "run";
}

static void names_a_member_twice(probe_description *probe) {
    probe->item[1].name = "label";
}

static void names_an_enum_entry_twice(probe_description *probe) {
    probe->modes[1].name = "PROBE_FAST";
}

static void names_a_parameter_twice(probe_description *probe) {
    probe->run[3].name = "input";
}

static void reaches_an_element_from_a_scalar(probe_description *probe) {
    probe->input[0].element = WEIVEL_STANDARD_DESCRIBED_U32;
}

static void types_a_text(probe_description *probe) {
    probe->item[0].type = "probe_item";
}

static void counts_a_scalar(probe_description *probe) {
    probe->input[2].count = "tag_count";
}

static void releases_a_text_member(probe_description *probe) {
    probe->input[4].release = "release_answer";
}

static void releases_a_text_input(probe_description *probe) {
    probe->get[1].release = "release_answer";
}

static void reaches_an_element_from_a_size_parameter(probe_description *probe) {
    probe->get[3].element = WEIVEL_STANDARD_DESCRIBED_U32;
}

static void releases_a_handle_through_an_operation(probe_description *probe) {
    probe->answer[1].release = "run";
}

static void releases_an_answer_through_an_operation(probe_description *probe) {
    probe->run[2].release = "get";
}

static void holds_a_context_outside_the_definition(probe_description *probe) {
    probe->answer[2].kind = WEIVEL_STANDARD_DESCRIBED_CONTEXT;
}

static void holds_two_contexts(probe_description *probe) {
    static weivel_standard_described_member three[3];
    three[0] = probe->definition[0];
    three[1] = probe->definition[1];
    three[2] = probe->definition[1];
    three[2].name = "second_context";
    three[2].offset = offsetof(probe_definition, run);
    probe->types[PROBE_DEFINITION_TYPE].members = three;
    probe->types[PROBE_DEFINITION_TYPE].member_count = 3;
}

static void holds_the_context_past_the_first_shape(probe_description *probe) {
    probe->types[PROBE_DEFINITION_TYPE].first_shape = offsetof(probe_definition, context);
}

static void names_a_member_as_an_operation(probe_description *probe) {
    probe->definition[1].name = "run";
}

static void lays_an_operation_over_a_member(probe_description *probe) {
    probe->operations[PROBE_RUN].offset = offsetof(probe_definition, context);
}

static void lays_two_operations_at_one_offset(probe_description *probe) {
    probe->operations[PROBE_GET].offset = offsetof(probe_definition, run);
}

static void takes_no_parameters(probe_description *probe) {
    probe->operations[PROBE_RELEASE_WALK].parameter_count = 0;
}

static void takes_the_context_second(probe_description *probe) {
    weivel_standard_described_parameter context = probe->release_walk[0];
    probe->release_walk[0] = probe->release_walk[1];
    probe->release_walk[1] = context;
}

static void takes_a_second_context(probe_description *probe) {
    probe->get[1].kind = WEIVEL_STANDARD_DESCRIBED_CONTEXT;
}

static void takes_a_struct_by_value(probe_description *probe) {
    probe->run[1].kind = WEIVEL_STANDARD_DESCRIBED_STRUCT;
    probe->run[1].element = WEIVEL_STANDARD_DESCRIBED_NONE;
}

static void answers_storage(probe_description *probe) {
    probe->get[4].kind = WEIVEL_STANDARD_DESCRIBED_STORAGE;
}

static void answers_a_pointer_to_a_value(probe_description *probe) {
    probe->run[2].element = WEIVEL_STANDARD_DESCRIBED_VALUE;
    probe->run[2].type = NULL;
}

static void refuses_through_a_text(probe_description *probe) {
    probe->run[3].kind = WEIVEL_STANDARD_DESCRIBED_TEXT;
    probe->run[3].element = WEIVEL_STANDARD_DESCRIBED_NONE;
    probe->run[3].type = NULL;
}

static void refuses_before_the_answer(probe_description *probe) {
    weivel_standard_described_parameter directory = probe->get[4];
    probe->get[4] = probe->get[5];
    probe->get[5] = directory;
}

static void releases_through_one_parameter(probe_description *probe) {
    probe->operations[PROBE_RELEASE_TREE].parameter_count = 1;
}

static void releases_a_text_answer(probe_description *probe) {
    probe->get[4].release = "release_answer";
}

static void releases_a_scalar_answer(probe_description *probe) {
    probe->get[4].kind = WEIVEL_STANDARD_DESCRIBED_U64;
    probe->get[4].release = "release_answer";
}

static void refuses_with_no_detail(probe_description *probe) {
    probe->refusal[1].name = "details";
}

static void refuses_with_no_fix_text(probe_description *probe) {
    probe->refusal[2].kind = WEIVEL_STANDARD_DESCRIBED_STORAGE;
}

static void blocks_in_no_operation(probe_description *probe) {
    probe->blocking[0] = "probe_wait";
}

static void blocks_in_a_release(probe_description *probe) {
    probe->blocking[0] = "release_tree";
}

static void blocks_twice(probe_description *probe) {
    static const char *twice[2] = {"lend", "lend"};
    probe->description.blocking = twice;
    probe->description.blocking_count = 2;
}

static void blocks_in_nothing_named(probe_description *probe) {
    probe->blocking[0] = NULL;
}

static void counts_blocking_it_does_not_hold(probe_description *probe) {
    probe->description.blocking = NULL;
}

static void defines_no_nested(probe_description *probe) {
    probe->loan[1].type = "probe_meter";
}

static void answers_a_definition_no_nested_defines(probe_description *probe) {
    probe->borrow[1].type = "probe_meter";
}

static void leaves_a_definition_untyped(probe_description *probe) {
    probe->loan[1].type = NULL;
}

static void nests_one_definition_twice(probe_description *probe) {
    static const weivel_standard_description *twice[2];
    twice[0] = &probe->counter;
    twice[1] = &probe->counter;
    probe->description.definitions = twice;
    probe->description.definition_count = 2;
}

static void nests_nothing(probe_description *probe) {
    probe->definitions[0] = NULL;
}

static void counts_definitions_it_does_not_hold(probe_description *probe) {
    probe->description.definitions = NULL;
}

static void nests_one_below_its_first_shape(probe_description *probe) {
    probe->counter.size = (uint32_t)offsetof(weivel_standard_description, operation_count);
}

static void nests_one_naming_no_definition(probe_description *probe) {
    probe->counter.definition = NULL;
}

static void nests_one_with_a_type_unnamed(probe_description *probe) {
    probe->counter_types[0].name = NULL;
}

static void nests_one_of_a_kind_past_the_last(probe_description *probe) {
    probe->add[1].kind = WEIVEL_STANDARD_DESCRIBED_LAST + 1;
}

static void nests_one_blocking_in_no_operation(probe_description *probe) {
    static const char *const waits[1] = {"probe_wait"};
    probe->counter.blocking = waits;
    probe->counter.blocking_count = 1;
}

static void nests_itself(probe_description *probe) {
    probe->counter.definitions = probe->definitions;
    probe->counter.definition_count = 1;
}

static void nests_the_description_holding_it(probe_description *probe) {
    static const weivel_standard_description *outer[1];
    outer[0] = &probe->description;
    probe->counter.definitions = outer;
    probe->counter.definition_count = 1;
}

#define CHAIN_LENGTH (WEIVEL_STANDARD_DESCRIPTION_DEPTH_LIMIT + 1)

/** Copies of the counter, each nesting the next, so no pointer repeats and only the depth bound ends the chain. */
static void nests_past_the_depth_limit(probe_description *probe) {
    static weivel_standard_description chain[CHAIN_LENGTH];
    static const weivel_standard_description *links[CHAIN_LENGTH];
    size_t at;
    for (at = 0; at < CHAIN_LENGTH; at++) {
        chain[at] = probe->counter;
        links[at] = &chain[at];
    }
    for (at = 0; at + 1 < CHAIN_LENGTH; at++) {
        chain[at].definitions = &links[at + 1];
        chain[at].definition_count = 1;
    }
    probe->definitions[0] = &chain[0];
}

static void releases_a_definition(probe_description *probe) {
    probe->release_answer[1].kind = WEIVEL_STANDARD_DESCRIBED_DEFINITION;
    probe->release_answer[1].element = WEIVEL_STANDARD_DESCRIBED_NONE;
    probe->release_answer[1].type = "probe_counter";
}

static void reaches_an_element_from_a_definition(probe_description *probe) {
    probe->loan[1].element = WEIVEL_STANDARD_DESCRIBED_STRUCT;
}

static void counts_a_definition(probe_description *probe) {
    probe->loan[1].count = "size";
}

static void releases_a_definition_member(probe_description *probe) {
    probe->loan[1].release = "release_answer";
}

static void releases_a_definition_answer(probe_description *probe) {
    probe->borrow[1].release = "release_answer";
}

static void takes_a_definition_as_an_input(probe_description *probe) {
    probe->borrow[1].role = WEIVEL_STANDARD_DESCRIBED_INPUT;
}

static void refuses_through_a_definition(probe_description *probe) {
    probe->borrow[1].role = WEIVEL_STANDARD_DESCRIBED_REFUSAL;
}

#define NESTED_NINE_DEEP                                                                                       \
    "definitions[0].definitions[0].definitions[0].definitions[0].definitions[0].definitions[0].definitions[0]." \
    "definitions[0].definitions[0]"

static const refused_case REFUSED[] = {
    {"a struct holding itself by value", holds_itself_by_value, INVALID, "types[0].members[0].type",
     "names a struct that is, or holds by value, the struct holding it"},
    {"two structs holding each other by value", holds_by_value_one_holding_it, INVALID, "types[0].members[0].type",
     "names a struct that is, or holds by value, the struct holding it"},
    {"a blocking name naming no operation", blocks_in_no_operation, INVALID, "blocking[0]",
     "names an operation the description does not hold"},
    {"a blocking name naming a release", blocks_in_a_release, INVALID, "blocking[0]", "names a release"},
    {"a blocking name listed twice", blocks_twice, DUPLICATE, "blocking[1]", "repeats a name listed before it"},
    {"a blocking name that is NULL", blocks_in_nothing_named, MISSING, "blocking[0]",
     "is NULL, and a description requires it"},
    {"blocking counted and not held", counts_blocking_it_does_not_hold, MISSING, "blocking",
     "is NULL beside a count above zero"},
    {"a definition member no nested description defines", defines_no_nested, INVALID, "types[5].members[1].type",
     "names a definition no description of `definitions` defines"},
    {"a definition answer no nested description defines", answers_a_definition_no_nested_defines, INVALID,
     "operations[8].parameters[1].type", "names a definition no description of `definitions` defines"},
    {"a definition with no type", leaves_a_definition_untyped, MISSING, "types[5].members[1].type",
     "is NULL, and a description requires it"},
    {"two nested descriptions of one definition", nests_one_definition_twice, DUPLICATE,
     "definitions[1].definition", "repeats the definition of a description listed before it"},
    {"a nested description that is NULL", nests_nothing, MISSING, "definitions[0]",
     "is NULL, and a description requires it"},
    {"definitions counted and not held", counts_definitions_it_does_not_hold, MISSING, "definitions",
     "is NULL beside a count above zero"},
    {"a nested description below its first shape", nests_one_below_its_first_shape, INVALID,
     "definitions[0].size", "is below the description's first shape"},
    {"a nested description naming no definition", nests_one_naming_no_definition, MISSING,
     "definitions[0].definition", "is NULL, and a description requires it"},
    {"a nested description with a type unnamed", nests_one_with_a_type_unnamed, MISSING,
     "definitions[0].types[0].name", "is NULL, and a description requires it"},
    {"a nested description of a kind past the last", nests_one_of_a_kind_past_the_last, INVALID,
     "definitions[0].operations[0].parameters[1].kind", "is not a kind weivel/description.h publishes"},
    {"a nested description blocking in no operation", nests_one_blocking_in_no_operation, INVALID,
     "definitions[0].blocking[0]", "names an operation the description does not hold"},
    {"a nested description nesting itself", nests_itself, INVALID, "definitions[0].definitions[0]",
     "is a description that holds it"},
    {"a nested description nesting the one holding it", nests_the_description_holding_it, INVALID,
     "definitions[0].definitions[0]", "is a description that holds it"},
    {"descriptions nested past the depth limit", nests_past_the_depth_limit, INVALID, NESTED_NINE_DEEP,
     "nests deeper than WEIVEL_STANDARD_DESCRIPTION_DEPTH_LIMIT"},
    {"a release freeing a definition", releases_a_definition, INVALID, "operations[3].parameters[1].kind",
     "is DEFINITION in a release"},
    {"an element on a definition", reaches_an_element_from_a_definition, INVALID, "types[5].members[1].element",
     "its kind leaves it unused"},
    {"a count on a definition", counts_a_definition, INVALID, "types[5].members[1].count",
     "its kind leaves it unused"},
    {"a release on a definition member", releases_a_definition_member, INVALID, "types[5].members[1].release",
     "its kind leaves it unused"},
    {"a release on a definition answer", releases_a_definition_answer, INVALID, "operations[8].parameters[1].release",
     "its kind leaves it unused"},
    {"a definition input", takes_a_definition_as_an_input, INVALID, "operations[8].parameters[1].kind",
     "a kind an input does not take"},
    {"a definition refusal", refuses_through_a_definition, INVALID, "operations[8].parameters[1].kind",
     "not a POINTER of STRUCT"},
    {"an element on a scalar member", reaches_an_element_from_a_scalar, INVALID, "types[1].members[0].element",
     "its kind leaves it unused"},
    {"a type on a text member", types_a_text, INVALID, "types[0].members[0].type", "its kind leaves it unused"},
    {"a count on a kind that counts nothing", counts_a_scalar, INVALID, "types[1].members[2].count",
     "its kind leaves it unused"},
    {"a release on a member that is no handle", releases_a_text_member, INVALID, "types[1].members[4].release",
     "its kind leaves it unused"},
    {"a release on an input that is no handle", releases_a_text_input, INVALID, "operations[1].parameters[1].release",
     "its kind leaves it unused"},
    {"an element on a size parameter", reaches_an_element_from_a_size_parameter, INVALID,
     "operations[1].parameters[3].element", "its kind leaves it unused"},
    {"a handle released by a status operation", releases_a_handle_through_an_operation, INVALID,
     "types[2].members[1].release", "names an operation answering a status"},
    {"an answer released by a status operation", releases_an_answer_through_an_operation, INVALID,
     "operations[0].parameters[2].release", "names an operation answering a status"},
    {"a context outside the definition", holds_a_context_outside_the_definition, INVALID,
     "types[2].members[2].kind", "CONTEXT outside the definition's struct"},
    {"two contexts in the definition", holds_two_contexts, INVALID, "types[3].members[2].kind",
     "a second CONTEXT"},
    {"a context past the definition's first shape", holds_the_context_past_the_first_shape, INVALID,
     "types[3].members[1].offset", "past the definition's first shape"},
    {"a member and an operation of one name", names_a_member_as_an_operation, DUPLICATE, "operations[0].name",
     "repeats the name of a member of the definition's struct"},
    {"an operation over a data member", lays_an_operation_over_a_member, INVALID, "operations[0].offset",
     "overlaps a data member"},
    {"two operations at one offset", lays_two_operations_at_one_offset, INVALID, "operations[1].offset",
     "overlaps an operation listed before it"},
    {"an operation of no parameters", takes_no_parameters, INVALID, "operations[5].parameters",
     "every operation takes the context first"},
    {"an operation taking the context second", takes_the_context_second, INVALID,
     "operations[5].parameters[0].kind", "is not CONTEXT"},
    {"a second context parameter", takes_a_second_context, INVALID, "operations[1].parameters[1].kind",
     "CONTEXT past the first parameter"},
    {"a struct input by value", takes_a_struct_by_value, INVALID, "operations[0].parameters[1].kind",
     "a kind an input does not take"},
    {"a storage answer", answers_storage, INVALID, "operations[1].parameters[4].kind",
     "a kind an answer does not take"},
    {"an answer pointing at a value", answers_a_pointer_to_a_value, INVALID, "operations[0].parameters[2].element",
     "VALUE on a pointer answer"},
    {"a refusal that is a text", refuses_through_a_text, INVALID, "operations[0].parameters[3].kind",
     "not a POINTER of STRUCT"},
    {"a refusal before the last parameter", refuses_before_the_answer, INVALID, "operations[1].parameters[4].role",
     "a REFUSAL before the operation's last parameter"},
    {"a release of one parameter", releases_through_one_parameter, INVALID, "operations[4].parameters",
     "is not two parameters"},
    {"a text answer naming a release", releases_a_text_answer, INVALID, "operations[1].parameters[4].release",
     "a scalar or a text answer"},
    {"a scalar answer naming a release", releases_a_scalar_answer, INVALID, "operations[1].parameters[4].release",
     "a scalar or a text answer"},
    {"a refusal carrying no detail", refuses_with_no_detail, INVALID, "operations[0].parameters[3].type",
     "no TEXT member named detail"},
    {"a refusal carrying no fix text", refuses_with_no_fix_text, INVALID, "operations[0].parameters[3].type",
     "no TEXT member named fix"},
    {"a struct the description does not hold", names_no_struct, INVALID, "types[1].members[9].type",
     "names a struct the description does not hold"},
    {"a pointer parameter to a struct it does not hold", points_at_no_struct, INVALID,
     "operations[0].parameters[1].type",
     "names a struct the description does not hold"},
    {"an enum the description does not hold", names_no_enum, INVALID, "types[1].members[1].type",
     "names an enum the description does not hold"},
    {"a definition the description does not hold", defines_no_struct, INVALID, "definition",
     "names a struct the description does not hold"},
    {"a member outside its struct", puts_a_member_outside, INVALID, "types[0].members[1].offset",
     "puts the member past the end of its struct"},
    {"a member of the wrong size for its kind", sizes_a_member_wrongly, INVALID, "types[1].members[1].size",
     "is not the size a member of its kind has"},
    {"an array with no count", counts_with_nothing, MISSING, "types[1].members[11].count",
     "is NULL, and a description requires it"},
    {"an array counted by a member that does not exist", counts_with_a_missing_member, INVALID,
     "types[1].members[11].count",
     "names nothing beside it"},
    {"texts counted by a member of another struct", counts_with_a_member_of_another_struct, INVALID,
     "types[1].members[5].count",
     "names nothing beside it"},
    {"an array counted by a member that is not a size", counts_with_a_member_not_a_size, INVALID,
     "types[1].members[11].count",
     "is not of kind SIZE"},
    {"bytes counted by a parameter that does not exist", counts_bytes_with_a_missing_parameter, INVALID,
     "operations[1].parameters[2].count",
     "names nothing beside it"},
    {"bytes counted by a parameter that is not a size", counts_bytes_with_a_parameter_not_a_size, INVALID,
     "operations[1].parameters[2].count",
     "is not of kind SIZE"},
    {"an operation outside the definition", puts_an_operation_outside, INVALID, "operations[6].offset",
     "past the end of the definition's struct"},
    {"a first shape past its struct", shapes_past_the_struct, INVALID, "types[1].first_shape",
     "lies past the end of its struct"},
    {"a member of no kind", gives_no_kind, INVALID, "types[1].members[2].kind",
     "is not a kind weivel/description.h publishes"},
    {"a member of a kind past the last", gives_a_kind_past_the_last, INVALID, "types[2].members[2].kind",
     "is not a kind weivel/description.h publishes"},
    {"a parameter of a kind past the last", gives_a_parameter_a_kind_past_the_last, INVALID,
     "operations[1].parameters[1].kind",
     "is not a kind weivel/description.h publishes"},
    {"a pointer at a scalar", points_at_a_scalar, INVALID, "types[1].members[10].element",
     "is not STRUCT or VALUE"},
    {"an array of arrays", holds_arrays_in_an_array, INVALID, "types[1].members[13].element",
     "is not a scalar, STRUCT or VALUE"},
    {"a parameter of no role", gives_no_role, INVALID, "operations[0].parameters[2].role",
     "is not a role weivel/description.h publishes"},
    {"a parameter of a role past the last", gives_a_role_past_the_last, INVALID, "operations[0].parameters[2].role",
     "is not a role weivel/description.h publishes"},
    {"an operation of no return", gives_no_return, INVALID, "operations[1].returns",
     "is not a return weivel/description.h publishes"},
    {"an operation of a return past the last", gives_a_return_past_the_last, INVALID, "operations[1].returns",
     "is not a return weivel/description.h publishes"},
    {"an answer released by an operation it does not hold", releases_through_nothing, INVALID,
     "operations[0].parameters[2].release",
     "names an operation the description does not hold"},
    {"a handle released by an operation it does not hold", releases_a_handle_through_nothing, INVALID,
     "types[2].members[1].release",
     "names an operation the description does not hold"},
    {"a handle with no release", leaves_a_handle_unreleased, MISSING, "types[2].members[1].release",
     "is NULL, and a description requires it"},
    {"a handle with no type", leaves_a_handle_untyped, MISSING, "types[2].members[1].type",
     "is NULL, and a description requires it"},
    {"a struct member with no type", leaves_a_struct_untyped, MISSING, "types[1].members[9].type",
     "is NULL, and a description requires it"},
    {"no definition", names_no_definition, MISSING, "definition",
     "is NULL, and a description requires it"},
    {"a type with no name", leaves_a_type_unnamed, MISSING, "types[2].name",
     "is NULL, and a description requires it"},
    {"a member with no name", leaves_a_member_unnamed, MISSING, "types[4].members[3].name",
     "is NULL, and a description requires it"},
    {"an enum with no name", leaves_an_enum_unnamed, MISSING, "enums[0].name",
     "is NULL, and a description requires it"},
    {"an enum entry with no name", leaves_an_enum_entry_unnamed, MISSING, "enums[0].entries[1].name",
     "is NULL, and a description requires it"},
    {"an operation with no name", leaves_an_operation_unnamed, MISSING, "operations[2].name",
     "is NULL, and a description requires it"},
    {"a parameter with no name", leaves_a_parameter_unnamed, MISSING, "operations[4].parameters[1].name",
     "is NULL, and a description requires it"},
    {"types counted and not held", counts_types_it_does_not_hold, MISSING, "types",
     "is NULL beside a count above zero"},
    {"members counted and not held", counts_members_it_does_not_hold, MISSING, "types[0].members",
     "is NULL beside a count above zero"},
    {"parameters counted and not held", counts_parameters_it_does_not_hold, MISSING, "operations[0].parameters",
     "is NULL beside a count above zero"},
    {"texts as an array of text", holds_texts_in_an_array, INVALID, "types[1].members[13].element",
     "a text array being TEXTS"},
    {"a value input by value", takes_a_value_by_value, INVALID, "operations[2].parameters[1].kind",
     "is VALUE on an input"},
    {"two types of one name", names_a_type_twice, DUPLICATE, "types[3].name", "repeats a name listed before it"},
    {"two enums of one name", names_an_enum_twice, DUPLICATE, "enums[1].name", "repeats a name listed before it"},
    {"two operations of one name", names_an_operation_twice, DUPLICATE, "operations[4].name",
     "repeats a name listed before it"},
    {"two members of one name", names_a_member_twice, DUPLICATE, "types[0].members[1].name",
     "repeats a name listed before it"},
    {"two enum entries of one name", names_an_enum_entry_twice, DUPLICATE, "enums[0].entries[1].name",
     "repeats a name listed before it"},
    {"two parameters of one name", names_a_parameter_twice, DUPLICATE, "operations[0].parameters[3].name",
     "repeats a name listed before it"},
};

/** A description of the first shape alone reads `blocking` and `definitions` as empty, never past its size. */
static void reads_an_older_description_as_holding_no_blocking_and_no_definitions(void) {
    probe_description probe;
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    probe_describe(&probe);
    probe.blocking[0] = NULL;
    probe.description.size = (uint32_t)(offsetof(weivel_standard_description, operation_count) + sizeof(size_t));
    CHECK(weivel_standard_description_check(&probe.description, &refusal) == WEIVEL_STATUS_MALFORMED);
    check_text(refusal.path, "types[5].members[1].type", "an older description holds no definitions");
    weivel_standard_refusal_release(&refusal);
}

static void refuses_by_path(const refused_case *item) {
    probe_description probe;
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    weivel_status status;
    probe_describe(&probe);
    item->breaks(&probe);
    status = weivel_standard_description_check(&probe.description, &refusal);
    if (status != WEIVEL_STATUS_MALFORMED || refusal.code != item->code) {
        fprintf(stderr, "%s: status %d code %u, wanted malformed with %u\n", item->name, (int)status,
                (unsigned)refusal.code, (unsigned)item->code);
        check_failures++;
    }
    check_text(refusal.path, item->path, item->name);
    if (refusal.detail == NULL || strstr(refusal.detail, item->says) == NULL) {
        fprintf(stderr, "%s: detail \"%s\" does not say \"%s\"\n", item->name,
                refusal.detail == NULL ? "(null)" : refusal.detail, item->says);
        check_failures++;
    }
    CHECK(refusal.fix != NULL && refusal.fix[0] != '\0');
    weivel_standard_refusal_release(&refusal);
    CHECK(weivel_standard_description_check(&probe.description, NULL) == WEIVEL_STATUS_MALFORMED);
}

static void refuses_a_malformed_call(void) {
    probe_description probe;
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    weivel_standard_refusal small = WEIVEL_INIT(weivel_standard_refusal);
    probe_describe(&probe);
    CHECK(weivel_standard_description_check(NULL, &refusal) == WEIVEL_STATUS_MALFORMED);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_MALFORMED_CALL);
    check_text(refusal.path, "", "a NULL description is refused at the root");
    probe.description.definition = NULL;
    CHECK(weivel_standard_description_check(&probe.description, &refusal) == WEIVEL_STATUS_MALFORMED);
    check_text(refusal.path, "", "a refusal still holding one is left as it was");
    weivel_standard_refusal_release(&refusal);
    probe_describe(&probe);
    probe.description.size = (uint32_t)offsetof(weivel_standard_description, operation_count);
    CHECK(weivel_standard_description_check(&probe.description, &refusal) == WEIVEL_STATUS_MALFORMED);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_MALFORMED_CALL);
    weivel_standard_refusal_release(&refusal);
    CHECK(weivel_standard_description_check(&probe.description, NULL) == WEIVEL_STATUS_MALFORMED);
    small.size = (uint32_t)offsetof(weivel_standard_refusal, storage);
    probe.description.size = (uint32_t)sizeof probe.description;
    probe.description.definition = NULL;
    CHECK(weivel_standard_description_check(&probe.description, &small) == WEIVEL_STATUS_MALFORMED);
    CHECK(small.code == WEIVEL_STANDARD_REFUSAL_NONE && small.detail == NULL);
}

int main(void) {
    size_t at;
    accepts_a_description_of_every_kind();
    accepts_a_struct_reaching_itself_through_a_pointer();
    accepts_a_struct_filled_by_another_holding_it_nowhere();
    checks_chains_of_structs_held_by_value();
    reads_a_newer_description_up_to_what_it_knows();
    reads_an_older_description_as_holding_no_blocking_and_no_definitions();
    for (at = 0; at < sizeof REFUSED / sizeof REFUSED[0]; at++) {
        refuses_by_path(&REFUSED[at]);
    }
    refuses_a_malformed_call();
    return check_summary("description_test");
}
