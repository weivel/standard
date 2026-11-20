#include "description_fixture.h"

#include <string.h>

#define KIND(name) WEIVEL_STANDARD_DESCRIBED_##name
#define NO_KIND WEIVEL_STANDARD_DESCRIBED_NONE

#define MEMBER(type, field, kind, element, named, count, release) \
    {#field, offsetof(type, field), sizeof(((type *)0)->field), kind, element, named, count, release}

#define PARAMETER(name, kind, role, element, named, count, release) \
    {name, kind, WEIVEL_STANDARD_DESCRIBED_##role, element, named, count, release}

#define REFUSAL_PARAMETER \
    PARAMETER("refusal", KIND(POINTER), REFUSAL, KIND(STRUCT), "weivel_standard_service_refusal", NULL, \
              "release_refusal")

#define CONTEXT_PARAMETER PARAMETER("context", KIND(CONTEXT), INPUT, NO_KIND, NULL, NULL, NULL)

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

static const weivel_standard_described_member ITEM[] = {
    MEMBER(probe_item, label, KIND(TEXT), NO_KIND, NULL, NULL, NULL),
    MEMBER(probe_item, weight, KIND(U64), NO_KIND, NULL, NULL, NULL)};

static const weivel_standard_described_member INPUT[] = {
    MEMBER(probe_input, size, KIND(U32), NO_KIND, NULL, NULL, NULL),
    MEMBER(probe_input, mode, KIND(U32), NO_KIND, "probe_mode", NULL, NULL),
    MEMBER(probe_input, delta, KIND(I64), NO_KIND, NULL, NULL, NULL),
    MEMBER(probe_input, ratio, KIND(F64), NO_KIND, NULL, NULL, NULL),
    MEMBER(probe_input, label, KIND(TEXT), NO_KIND, NULL, NULL, NULL),
    MEMBER(probe_input, tags, KIND(TEXTS), NO_KIND, NULL, "tag_count", NULL),
    MEMBER(probe_input, tag_count, KIND(SIZE), NO_KIND, NULL, NULL, NULL),
    MEMBER(probe_input, bytes, KIND(BYTES), NO_KIND, NULL, "length", NULL),
    MEMBER(probe_input, length, KIND(SIZE), NO_KIND, NULL, NULL, NULL),
    MEMBER(probe_input, first, KIND(STRUCT), NO_KIND, "probe_item", NULL, NULL),
    MEMBER(probe_input, best, KIND(POINTER), KIND(STRUCT), "probe_item", NULL, NULL),
    MEMBER(probe_input, items, KIND(ARRAY), KIND(STRUCT), "probe_item", "item_count", NULL),
    MEMBER(probe_input, item_count, KIND(SIZE), NO_KIND, NULL, NULL, NULL),
    MEMBER(probe_input, numbers, KIND(ARRAY), KIND(U32), NULL, "number_count", NULL),
    MEMBER(probe_input, number_count, KIND(SIZE), NO_KIND, NULL, NULL, NULL),
    MEMBER(probe_input, extra, KIND(POINTER), KIND(VALUE), NULL, NULL, NULL),
    MEMBER(probe_input, previous, KIND(VALUE), NO_KIND, NULL, NULL, NULL)};

static const weivel_standard_described_member ANSWER[] = {
    MEMBER(probe_answer, size, KIND(U32), NO_KIND, NULL, NULL, NULL),
    MEMBER(probe_answer, walk, KIND(HANDLE), NO_KIND, "probe_handle", NULL, "release_walk"),
    MEMBER(probe_answer, storage, KIND(STORAGE), NO_KIND, NULL, NULL, NULL)};

static const weivel_standard_described_member DEFINITION[] = {
    MEMBER(probe_definition, size, KIND(U32), NO_KIND, NULL, NULL, NULL),
    MEMBER(probe_definition, context, KIND(CONTEXT), NO_KIND, NULL, NULL, NULL)};

static const weivel_standard_described_member REFUSAL[] = {
    MEMBER(weivel_standard_service_refusal, size, KIND(U32), NO_KIND, NULL, NULL, NULL),
    MEMBER(weivel_standard_service_refusal, detail, KIND(TEXT), NO_KIND, NULL, NULL, NULL),
    MEMBER(weivel_standard_service_refusal, fix, KIND(TEXT), NO_KIND, NULL, NULL, NULL),
    MEMBER(weivel_standard_service_refusal, storage, KIND(STORAGE), NO_KIND, NULL, NULL, NULL)};

static const weivel_standard_described_enum_entry MODES[] = {{"PROBE_FAST", PROBE_FAST}, {"PROBE_SLOW", PROBE_SLOW}};

static const weivel_standard_described_parameter RUN[] = {
    CONTEXT_PARAMETER,
    PARAMETER("input", KIND(POINTER), INPUT, KIND(STRUCT), "probe_input", NULL, NULL),
    PARAMETER("answer", KIND(POINTER), ANSWER, KIND(STRUCT), "probe_answer", NULL, "release_answer"),
    REFUSAL_PARAMETER};

static const weivel_standard_described_parameter GET[] = {
    CONTEXT_PARAMETER,
    PARAMETER("key", KIND(TEXT), INPUT, NO_KIND, NULL, NULL, NULL),
    PARAMETER("bytes", KIND(BYTES), INPUT, NO_KIND, NULL, "length", NULL),
    PARAMETER("length", KIND(SIZE), INPUT, NO_KIND, NULL, NULL, NULL),
    PARAMETER("directory", KIND(TEXT), ANSWER, NO_KIND, NULL, NULL, NULL),
    REFUSAL_PARAMETER};

static const weivel_standard_described_parameter READ[] = {
    CONTEXT_PARAMETER,
    PARAMETER("query", KIND(POINTER), INPUT, KIND(VALUE), NULL, NULL, NULL),
    PARAMETER("tree", KIND(VALUE), ANSWER, NO_KIND, NULL, NULL, "release_tree"),
    REFUSAL_PARAMETER};

static const weivel_standard_described_parameter RELEASE_ANSWER[] = {
    CONTEXT_PARAMETER, PARAMETER("answer", KIND(POINTER), ANSWER, KIND(STRUCT), "probe_answer", NULL, NULL)};

static const weivel_standard_described_parameter RELEASE_TREE[] = {
    CONTEXT_PARAMETER, PARAMETER("tree", KIND(VALUE), ANSWER, NO_KIND, NULL, NULL, NULL)};

static const weivel_standard_described_parameter RELEASE_WALK[] = {
    CONTEXT_PARAMETER, PARAMETER("walk", KIND(HANDLE), INPUT, NO_KIND, "probe_handle", NULL, "release_walk")};

static const weivel_standard_described_parameter RELEASE_REFUSAL[] = {
    CONTEXT_PARAMETER,
    PARAMETER("refusal", KIND(POINTER), REFUSAL, KIND(STRUCT), "weivel_standard_service_refusal", NULL, NULL)};

static const weivel_standard_described_member LOAN[] = {
    MEMBER(probe_loan, size, KIND(U32), NO_KIND, NULL, NULL, NULL),
    MEMBER(probe_loan, counter, KIND(DEFINITION), NO_KIND, "probe_counter", NULL, NULL)};

static const weivel_standard_described_parameter LEND[] = {
    CONTEXT_PARAMETER, PARAMETER("loan", KIND(POINTER), ANSWER, KIND(STRUCT), "probe_loan", NULL, NULL)};

static const weivel_standard_described_parameter BORROW[] = {
    CONTEXT_PARAMETER, PARAMETER("counter", KIND(DEFINITION), ANSWER, NO_KIND, "probe_counter", NULL, NULL)};

static const weivel_standard_described_member COUNTER[] = {
    MEMBER(probe_counter, size, KIND(U32), NO_KIND, NULL, NULL, NULL),
    MEMBER(probe_counter, context, KIND(CONTEXT), NO_KIND, NULL, NULL, NULL)};

static const weivel_standard_described_parameter ADD[] = {
    CONTEXT_PARAMETER, PARAMETER("amount", KIND(U64), INPUT, NO_KIND, NULL, NULL, NULL),
    PARAMETER("total", KIND(U64), ANSWER, NO_KIND, NULL, NULL, NULL)};

static weivel_standard_described_type type_of(const char *name, size_t size, size_t first_shape,
                                              const weivel_standard_described_member *members, size_t count) {
    weivel_standard_described_type type;
    type.name = name;
    type.size = size;
    type.first_shape = first_shape;
    type.members = members;
    type.member_count = count;
    return type;
}

static weivel_standard_described_operation operation_of(const char *name, size_t offset, uint32_t returns,
                                                        const weivel_standard_described_parameter *parameters,
                                                        size_t count) {
    weivel_standard_described_operation operation;
    operation.name = name;
    operation.offset = offset;
    operation.returns = returns;
    operation.parameters = parameters;
    operation.parameter_count = count;
    return operation;
}

#define COPY(field, source) memcpy(probe->field, source, sizeof probe->field)

static void copy_arrays(probe_description *probe) {
    COPY(item, ITEM);
    COPY(input, INPUT);
    COPY(answer, ANSWER);
    COPY(definition, DEFINITION);
    COPY(refusal, REFUSAL);
    COPY(modes, MODES);
    COPY(run, RUN);
    COPY(get, GET);
    COPY(read, READ);
    COPY(release_answer, RELEASE_ANSWER);
    COPY(release_tree, RELEASE_TREE);
    COPY(release_walk, RELEASE_WALK);
    COPY(release_refusal, RELEASE_REFUSAL);
    COPY(loan, LOAN);
    COPY(lend, LEND);
    COPY(borrow, BORROW);
    COPY(counter_members, COUNTER);
    COPY(add, ADD);
}

static void describe_types(probe_description *probe) {
    probe->types[PROBE_ITEM_TYPE] =
        type_of("probe_item", sizeof(probe_item), sizeof(probe_item), probe->item, COUNT_OF(probe->item));
    probe->types[PROBE_INPUT_TYPE] = type_of("probe_input", sizeof(probe_input), sizeof(probe_input), probe->input,
                                             COUNT_OF(probe->input));
    probe->types[PROBE_ANSWER_TYPE] = type_of("probe_answer", sizeof(probe_answer), sizeof(probe_answer),
                                              probe->answer, COUNT_OF(probe->answer));
    probe->types[PROBE_DEFINITION_TYPE] =
        type_of("probe_definition", sizeof(probe_definition), sizeof(probe_definition), probe->definition,
                COUNT_OF(probe->definition));
    probe->types[PROBE_REFUSAL_TYPE] =
        type_of("weivel_standard_service_refusal", sizeof(weivel_standard_service_refusal),
                sizeof(weivel_standard_service_refusal), probe->refusal, COUNT_OF(probe->refusal));
    probe->types[PROBE_LOAN_TYPE] =
        type_of("probe_loan", sizeof(probe_loan), sizeof(probe_loan), probe->loan, COUNT_OF(probe->loan));
    probe->enums[0].name = "probe_mode";
    probe->enums[0].entries = probe->modes;
    probe->enums[0].entry_count = COUNT_OF(probe->modes);
}

#define OPERATION(index, field, returns, parameters)                                                     \
    probe->operations[index] = operation_of(#field, offsetof(probe_definition, field),                    \
                                            WEIVEL_STANDARD_DESCRIBED_##returns, probe->parameters,      \
                                            COUNT_OF(probe->parameters))

static void describe_operations(probe_description *probe) {
    OPERATION(PROBE_RUN, run, STATUS, run);
    OPERATION(PROBE_GET, get, STATUS, get);
    OPERATION(PROBE_READ, read, STATUS, read);
    OPERATION(PROBE_RELEASE_ANSWER, release_answer, NOTHING, release_answer);
    OPERATION(PROBE_RELEASE_TREE, release_tree, NOTHING, release_tree);
    OPERATION(PROBE_RELEASE_WALK, release_walk, NOTHING, release_walk);
    OPERATION(PROBE_RELEASE_REFUSAL, release_refusal, NOTHING, release_refusal);
    OPERATION(PROBE_LEND, lend, STATUS, lend);
    OPERATION(PROBE_BORROW, borrow, STATUS, borrow);
}

static void describe_counter(probe_description *probe) {
    probe->counter_types[0] = type_of("probe_counter", sizeof(probe_counter), sizeof(probe_counter),
                                      probe->counter_members, COUNT_OF(probe->counter_members));
    probe->counter_operations[0] = operation_of("add", offsetof(probe_counter, add), WEIVEL_STANDARD_DESCRIBED_STATUS,
                                                probe->add, COUNT_OF(probe->add));
    probe->counter.size = (uint32_t)sizeof probe->counter;
    probe->counter.definition = "probe_counter";
    probe->counter.types = probe->counter_types;
    probe->counter.type_count = COUNT_OF(probe->counter_types);
    probe->counter.operations = probe->counter_operations;
    probe->counter.operation_count = COUNT_OF(probe->counter_operations);
    probe->definitions[0] = &probe->counter;
}

void probe_describe(probe_description *probe) {
    memset(probe, 0, sizeof *probe);
    copy_arrays(probe);
    describe_types(probe);
    describe_operations(probe);
    describe_counter(probe);
    probe->blocking[0] = "lend";
    probe->description.size = (uint32_t)sizeof probe->description;
    probe->description.definition = "probe_definition";
    probe->description.types = probe->types;
    probe->description.type_count = COUNT_OF(probe->types);
    probe->description.enums = probe->enums;
    probe->description.enum_count = COUNT_OF(probe->enums);
    probe->description.operations = probe->operations;
    probe->description.operation_count = COUNT_OF(probe->operations);
    probe->description.blocking = probe->blocking;
    probe->description.blocking_count = COUNT_OF(probe->blocking);
    probe->description.definitions = probe->definitions;
    probe->description.definition_count = COUNT_OF(probe->definitions);
}
