#ifndef WEIVEL_STANDARD_TEST_DESCRIPTION_FIXTURE_H
#define WEIVEL_STANDARD_TEST_DESCRIPTION_FIXTURE_H

#include "weivel/description.h"
#include "weivel/services.h"
#include "weivel/value.h"

typedef struct probe_handle probe_handle;

typedef enum probe_mode { PROBE_FAST = 1, PROBE_SLOW = 2 } probe_mode;

typedef struct probe_item {
    const char *label;
    uint64_t weight;
} probe_item;

typedef struct probe_input {
    uint32_t size;
    uint32_t mode;
    int64_t delta;
    double ratio;
    const char *label;
    const char *const *tags;
    size_t tag_count;
    const uint8_t *bytes;
    size_t length;
    probe_item first;
    const probe_item *best;
    const probe_item *items;
    size_t item_count;
    const uint32_t *numbers;
    size_t number_count;
    const weivel_standard_value *extra;
    weivel_standard_value previous;
} probe_input;

typedef struct probe_answer {
    uint32_t size;
    probe_handle *walk;
    void *storage;
} probe_answer;

typedef struct probe_definition {
    uint32_t size;
    void *context;
    weivel_status (*run)(void *context, const probe_input *input, probe_answer *answer,
                         weivel_standard_service_refusal *refusal);
    weivel_status (*get)(void *context, const char *key, const uint8_t *bytes, size_t length, const char **directory,
                         weivel_standard_service_refusal *refusal);
    weivel_status (*read)(void *context, const weivel_standard_value *query, weivel_standard_value_tree *tree,
                          weivel_standard_service_refusal *refusal);
    void (*release_answer)(void *context, probe_answer *answer);
    void (*release_tree)(void *context, weivel_standard_value_tree *tree);
    void (*release_walk)(void *context, probe_handle *walk);
    void (*release_refusal)(void *context, weivel_standard_service_refusal *refusal);
} probe_definition;

enum {
    PROBE_ITEM_TYPE,
    PROBE_INPUT_TYPE,
    PROBE_ANSWER_TYPE,
    PROBE_DEFINITION_TYPE,
    PROBE_REFUSAL_TYPE,
    PROBE_TYPE_COUNT
};

enum { PROBE_RUN, PROBE_GET, PROBE_READ, PROBE_RELEASE_ANSWER, PROBE_RELEASE_TREE, PROBE_RELEASE_WALK,
       PROBE_RELEASE_REFUSAL, PROBE_OPERATION_COUNT };

/**
 * The description of probe_definition, a definition written in the tests only, with a member of every kind and an
 * operation of every parameter form `weivel/services.h` uses, held in writable arrays so each test breaks one thing.
 */
typedef struct probe_description {
    weivel_standard_described_member item[2];
    weivel_standard_described_member input[17];
    weivel_standard_described_member answer[3];
    weivel_standard_described_member definition[2];
    weivel_standard_described_member refusal[4];
    weivel_standard_described_type types[PROBE_TYPE_COUNT];
    weivel_standard_described_enum_entry modes[2];
    weivel_standard_described_enum enums[1];
    weivel_standard_described_parameter run[4];
    weivel_standard_described_parameter get[6];
    weivel_standard_described_parameter read[4];
    weivel_standard_described_parameter release_answer[2];
    weivel_standard_described_parameter release_tree[2];
    weivel_standard_described_parameter release_walk[2];
    weivel_standard_described_parameter release_refusal[2];
    weivel_standard_described_operation operations[PROBE_OPERATION_COUNT];
    weivel_standard_description description;
} probe_description;

/** Fills `probe` with the whole, correct description of probe_definition. */
void probe_describe(probe_description *probe);

#endif
