/**
 * @file
 * The description another library would export for probe_remote, written by hand, which every source weivel-describe
 * writes for nested.h calls.
 */

#include <stddef.h>
#include <stdint.h>

#include "nested_remote.h"

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

static const weivel_standard_described_member REMOTE_MEMBERS[] = {
    {.name = "size",
     .offset = offsetof(probe_remote, size),
     .size = sizeof(((probe_remote *)0)->size),
     .kind = WEIVEL_STANDARD_DESCRIBED_U32,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL},
    {.name = "context",
     .offset = offsetof(probe_remote, context),
     .size = sizeof(((probe_remote *)0)->context),
     .kind = WEIVEL_STANDARD_DESCRIBED_CONTEXT,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL}};

static const weivel_standard_described_type REMOTE_TYPES[] = {
    {.name = "probe_remote",
     .size = sizeof(probe_remote),
     .first_shape = offsetof(probe_remote, ping) + sizeof(((probe_remote *)0)->ping),
     .members = REMOTE_MEMBERS,
     .member_count = COUNT_OF(REMOTE_MEMBERS)}};

static const weivel_standard_described_parameter PING_PARAMETERS[] = {
    {.name = "context",
     .kind = WEIVEL_STANDARD_DESCRIBED_CONTEXT,
     .role = WEIVEL_STANDARD_DESCRIBED_INPUT,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL},
    {.name = "pong",
     .kind = WEIVEL_STANDARD_DESCRIBED_U64,
     .role = WEIVEL_STANDARD_DESCRIBED_ANSWER,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL}};

static const weivel_standard_described_operation REMOTE_OPERATIONS[] = {
    {.name = "ping",
     .offset = offsetof(probe_remote, ping),
     .returns = WEIVEL_STANDARD_DESCRIBED_STATUS,
     .parameters = PING_PARAMETERS,
     .parameter_count = COUNT_OF(PING_PARAMETERS)}};

static const weivel_standard_description REMOTE = {.size = (uint32_t)sizeof(weivel_standard_description),
                                                    .definition = "probe_remote",
                                                    .types = REMOTE_TYPES,
                                                    .type_count = COUNT_OF(REMOTE_TYPES),
                                                    .enums = NULL,
                                                    .enum_count = 0,
                                                    .operations = REMOTE_OPERATIONS,
                                                    .operation_count = COUNT_OF(REMOTE_OPERATIONS),
                                                    .blocking = NULL,
                                                    .blocking_count = 0,
                                                    .definitions = NULL,
                                                    .definition_count = 0};

const weivel_standard_description *probe_remote_description(void) {
    return &REMOTE;
}
