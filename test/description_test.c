#include "check.h"
#include "description_fixture.h"

#define INVALID WEIVEL_STANDARD_REFUSAL_INVALID_VALUE
#define MISSING WEIVEL_STANDARD_REFUSAL_MISSING_FIELD

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

static const refused_case REFUSED[] = {
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
     "is not a scalar, TEXT, STRUCT or VALUE"},
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
};

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
    reads_a_newer_description_up_to_what_it_knows();
    for (at = 0; at < sizeof REFUSED / sizeof REFUSED[0]; at++) {
        refuses_by_path(&REFUSED[at]);
    }
    refuses_a_malformed_call();
    return check_summary("description_test");
}
