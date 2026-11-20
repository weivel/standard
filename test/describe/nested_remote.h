#ifndef PROBE_NESTED_REMOTE_H
#define PROBE_NESTED_REMOTE_H

/**
 * @file
 * A definition another library describes, as `probe_remote_description`: nested.h holds one, and weivel-describe is
 * told it is described elsewhere rather than scanning this header.
 */

#include <stdint.h>

#include "weivel/standard.h"

/** First shape ends at `ping`. */
typedef struct probe_remote {
    uint32_t size;
    void *context;
    weivel_status (*ping)(void *context, uint64_t *pong);
} probe_remote;

#endif
