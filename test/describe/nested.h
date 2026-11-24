#ifndef PROBE_NESTED_H
#define PROBE_NESTED_H

/**
 * @file
 * A definition answering two others, one declared here and one described elsewhere, and an operation that blocks.
 */

#include <stddef.h>
#include <stdint.h>

#include "nested_remote.h"
#include "weivel/standard.h"

#ifdef __cplusplus
extern "C" {
#endif

/** A counter, the definition a lease holds and `borrow` answers. First shape ends at `add`. */
typedef struct probe_counter {
    uint32_t size;
    void *context;
    /**
     * Adds to the counter.
     *
     * @param total answers the counter's total once `amount` is added. Required.
     */
    weivel_status (*add)(void *context, uint64_t amount, uint64_t *total);
} probe_counter;

/** What `lend` answers: a counter, and a remote another library describes. First shape ends at `remote`. */
typedef struct probe_lease {
    uint32_t size;
    const probe_counter *counter;
    const probe_remote *remote;
} probe_lease;

/** The lender. First shape ends at `borrow`. */
typedef struct probe_lender {
    uint32_t size;
    void *context;
    /**
     * Waits until a counter is free, and answers a lease of it.
     *
     * @blocking it waits for a counter another caller holds.
     * @param lease the consumer's empty answer. Required.
     */
    weivel_status (*lend)(void *context, probe_lease *lease);
    /**
     * The counter, at once.
     *
     * @param counter answers the counter, which lives as long as the lender. Required.
     */
    weivel_status (*borrow)(void *context, const probe_counter **counter);
} probe_lender;

/** Answers the lender, the function golden/scanned.c names, which this header declares so the source does not. */
const void *probe_lender_definition(void);

#ifdef __cplusplus
}
#endif

#endif
