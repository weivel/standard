/* Written by weivel-describe from the headers it includes; edit them, not this file. */

#include <stddef.h>
#include <stdint.h>

#include "weivel/description.h"
#include "weivel/services.h"

static const weivel_standard_described_member paths_weivel_standard_paths_members[] = {
    {.name = "size",
     .offset = offsetof(weivel_standard_paths, size),
     .size = sizeof(((weivel_standard_paths *)0)->size),
     .kind = WEIVEL_STANDARD_DESCRIBED_U32,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL},
    {.name = "context",
     .offset = offsetof(weivel_standard_paths, context),
     .size = sizeof(((weivel_standard_paths *)0)->context),
     .kind = WEIVEL_STANDARD_DESCRIBED_CONTEXT,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL}};

static const weivel_standard_described_parameter paths_cache_parameters[] = {
    {.name = "context",
     .kind = WEIVEL_STANDARD_DESCRIBED_CONTEXT,
     .role = WEIVEL_STANDARD_DESCRIBED_INPUT,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL},
    {.name = "directory",
     .kind = WEIVEL_STANDARD_DESCRIBED_TEXT,
     .role = WEIVEL_STANDARD_DESCRIBED_ANSWER,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL}};

static const weivel_standard_described_parameter paths_config_parameters[] = {
    {.name = "context",
     .kind = WEIVEL_STANDARD_DESCRIBED_CONTEXT,
     .role = WEIVEL_STANDARD_DESCRIBED_INPUT,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL},
    {.name = "directory",
     .kind = WEIVEL_STANDARD_DESCRIBED_TEXT,
     .role = WEIVEL_STANDARD_DESCRIBED_ANSWER,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL}};

static const weivel_standard_described_parameter paths_plugin_parameters[] = {
    {.name = "context",
     .kind = WEIVEL_STANDARD_DESCRIBED_CONTEXT,
     .role = WEIVEL_STANDARD_DESCRIBED_INPUT,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL},
    {.name = "directory",
     .kind = WEIVEL_STANDARD_DESCRIBED_TEXT,
     .role = WEIVEL_STANDARD_DESCRIBED_ANSWER,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL}};

static const weivel_standard_described_type paths_types[] = {
    {.name = "weivel_standard_paths",
     .size = sizeof(weivel_standard_paths),
     .first_shape = offsetof(weivel_standard_paths, plugin) + sizeof(((weivel_standard_paths *)0)->plugin),
     .members = paths_weivel_standard_paths_members,
     .member_count = sizeof(paths_weivel_standard_paths_members) / sizeof(paths_weivel_standard_paths_members[0])}};

static const weivel_standard_described_operation paths_operations[] = {
    {.name = "cache",
     .offset = offsetof(weivel_standard_paths, cache),
     .returns = WEIVEL_STANDARD_DESCRIBED_STATUS,
     .parameters = paths_cache_parameters,
     .parameter_count = sizeof(paths_cache_parameters) / sizeof(paths_cache_parameters[0])},
    {.name = "config",
     .offset = offsetof(weivel_standard_paths, config),
     .returns = WEIVEL_STANDARD_DESCRIBED_STATUS,
     .parameters = paths_config_parameters,
     .parameter_count = sizeof(paths_config_parameters) / sizeof(paths_config_parameters[0])},
    {.name = "plugin",
     .offset = offsetof(weivel_standard_paths, plugin),
     .returns = WEIVEL_STANDARD_DESCRIBED_STATUS,
     .parameters = paths_plugin_parameters,
     .parameter_count = sizeof(paths_plugin_parameters) / sizeof(paths_plugin_parameters[0])}};

static const weivel_standard_description paths_description = {
    .size = (uint32_t)sizeof(weivel_standard_description),
    .definition = "weivel_standard_paths",
    .types = paths_types,
    .type_count = sizeof(paths_types) / sizeof(paths_types[0]),
    .enums = NULL,
    .enum_count = 0,
    .operations = paths_operations,
    .operation_count = sizeof(paths_operations) / sizeof(paths_operations[0])};

WEIVEL_STANDARD_API const weivel_standard_description *describe_paths(void);

WEIVEL_STANDARD_API const weivel_standard_description *describe_paths(void) {
    return &paths_description;
}
