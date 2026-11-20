#ifndef PROBE_EVERY_FORM_H
#define PROBE_EVERY_FORM_H

/**
 * @file
 * A member of every kind and an operation of every parameter form, as test/description_fixture.h describes by hand.
 */

#include <stddef.h>
#include <stdint.h>

#include "weivel/core.h"
#include "weivel/services.h"
#include "weivel/value.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct probe_handle probe_handle;

/** How a probe runs. */
typedef enum probe_mode { PROBE_FAST = 1, PROBE_SLOW = 2 } probe_mode;

/** One item: no size, and it never grows. */
typedef struct probe_item {
    const char *label;
    uint64_t weight;
} probe_item;

/** What a run reads. First shape ends at `previous`. */
typedef struct probe_input {
    uint32_t size;
    /** A probe_mode. */
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

/** What a run answers. First shape ends at `storage`. */
typedef struct probe_answer {
    uint32_t size;
    probe_handle *walk;
    void *storage;
} probe_answer;

/** The probe service. First shape ends at `release_refusal`. */
typedef struct probe_definition {
    uint32_t size;
    void *context;
    weivel_status (*run)(void *context, const probe_input *input, probe_answer *answer,
                         weivel_standard_service_refusal *refusal);
    weivel_status (*get)(void *context, const char *key, const uint8_t *bytes, size_t length, const char **directory,
                         weivel_standard_service_refusal *refusal);
    weivel_status (*read)(void *context, const weivel_standard_value *query, weivel_standard_value_tree *tree,
                          weivel_standard_service_refusal *refusal);
    /**
     * Opens a walk over `items`.
     *
     * @param items read during the call only.
     * @param walk the consumer's empty handle. Required.
     * @param total answers how many there were.
     */
    weivel_status (*open)(void *context, const probe_item *items, size_t item_count, probe_handle **walk,
                          uint64_t *total);
    weivel_status (*step)(void *context, probe_handle *walk, weivel_standard_service_refusal *refusal);
    void (*release_answer)(void *context, probe_answer *answer);
    void (*release_tree)(void *context, weivel_standard_value_tree *tree);
    void (*release_walk)(void *context, probe_handle *walk);
    void (*release_refusal)(void *context, weivel_standard_service_refusal *refusal);
} probe_definition;

/** The probe's description. */
WEIVEL_STANDARD_API const weivel_standard_description *probe_description_of(void);

#ifdef __cplusplus
}
#endif

#endif
