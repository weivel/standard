#ifndef PROBE_NESTED_REMOTE_H
#define PROBE_NESTED_REMOTE_H

/**
 * @file
 * A definition another library describes, as `probe_remote_description`, which nested.h holds. weivel-describe is told
 * it is described elsewhere, whether this header is scanned or not, and never writes its description itself.
 */

#include <stdint.h>

#include "weivel/description.h"
#include "weivel/standard.h"

/** First shape ends at `ping`. */
typedef struct probe_remote {
    uint32_t size;
    void *context;
    weivel_status (*ping)(void *context, uint64_t *pong);
} probe_remote;

/** The description of probe_remote, which test/describe/remote.c answers. */
const weivel_standard_description *probe_remote_description(void);

#endif
