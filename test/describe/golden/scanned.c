/* Written by weivel-describe from the headers it includes; edit them, not this file. */

#include <stddef.h>
#include <stdint.h>

#include "weivel/description.h"
#include "nested_remote.h"
#include "nested.h"

static const weivel_standard_described_member scanned_probe_counter_probe_counter_members[] = {
    {.name = "size",
     .offset = offsetof(probe_counter, size),
     .size = sizeof(((probe_counter *)0)->size),
     .kind = WEIVEL_STANDARD_DESCRIBED_U32,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL},
    {.name = "context",
     .offset = offsetof(probe_counter, context),
     .size = sizeof(((probe_counter *)0)->context),
     .kind = WEIVEL_STANDARD_DESCRIBED_CONTEXT,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL}};

static const weivel_standard_described_parameter scanned_probe_counter_add_parameters[] = {
    {.name = "context",
     .kind = WEIVEL_STANDARD_DESCRIBED_CONTEXT,
     .role = WEIVEL_STANDARD_DESCRIBED_INPUT,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL},
    {.name = "amount",
     .kind = WEIVEL_STANDARD_DESCRIBED_U64,
     .role = WEIVEL_STANDARD_DESCRIBED_INPUT,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL},
    {.name = "total",
     .kind = WEIVEL_STANDARD_DESCRIBED_U64,
     .role = WEIVEL_STANDARD_DESCRIBED_ANSWER,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL}};

static const weivel_standard_described_type scanned_probe_counter_types[] = {
    {.name = "probe_counter",
     .size = sizeof(probe_counter),
     .first_shape = offsetof(probe_counter, add) + sizeof(((probe_counter *)0)->add),
     .members = scanned_probe_counter_probe_counter_members,
     .member_count = sizeof(scanned_probe_counter_probe_counter_members) / sizeof(scanned_probe_counter_probe_counter_members[0])}};

static const weivel_standard_described_operation scanned_probe_counter_operations[] = {
    {.name = "add",
     .offset = offsetof(probe_counter, add),
     .returns = WEIVEL_STANDARD_DESCRIBED_STATUS,
     .parameters = scanned_probe_counter_add_parameters,
     .parameter_count = sizeof(scanned_probe_counter_add_parameters) / sizeof(scanned_probe_counter_add_parameters[0])}};

static const weivel_standard_description scanned_probe_counter_description = {
    .size = (uint32_t)sizeof(weivel_standard_description),
    .definition = "probe_counter",
    .types = scanned_probe_counter_types,
    .type_count = sizeof(scanned_probe_counter_types) / sizeof(scanned_probe_counter_types[0]),
    .enums = NULL,
    .enum_count = 0,
    .operations = scanned_probe_counter_operations,
    .operation_count = sizeof(scanned_probe_counter_operations) / sizeof(scanned_probe_counter_operations[0]),
    .blocking = NULL,
    .blocking_count = 0,
    .definitions = NULL,
    .definition_count = 0};

static const weivel_standard_described_member scanned_probe_lease_members[] = {
    {.name = "size",
     .offset = offsetof(probe_lease, size),
     .size = sizeof(((probe_lease *)0)->size),
     .kind = WEIVEL_STANDARD_DESCRIBED_U32,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL},
    {.name = "counter",
     .offset = offsetof(probe_lease, counter),
     .size = sizeof(((probe_lease *)0)->counter),
     .kind = WEIVEL_STANDARD_DESCRIBED_DEFINITION,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = "probe_counter",
     .count = NULL,
     .release = NULL},
    {.name = "remote",
     .offset = offsetof(probe_lease, remote),
     .size = sizeof(((probe_lease *)0)->remote),
     .kind = WEIVEL_STANDARD_DESCRIBED_DEFINITION,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = "probe_remote",
     .count = NULL,
     .release = NULL}};

static const weivel_standard_described_member scanned_probe_lender_members[] = {
    {.name = "size",
     .offset = offsetof(probe_lender, size),
     .size = sizeof(((probe_lender *)0)->size),
     .kind = WEIVEL_STANDARD_DESCRIBED_U32,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL},
    {.name = "context",
     .offset = offsetof(probe_lender, context),
     .size = sizeof(((probe_lender *)0)->context),
     .kind = WEIVEL_STANDARD_DESCRIBED_CONTEXT,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL}};

static const weivel_standard_described_parameter scanned_lend_parameters[] = {
    {.name = "context",
     .kind = WEIVEL_STANDARD_DESCRIBED_CONTEXT,
     .role = WEIVEL_STANDARD_DESCRIBED_INPUT,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL},
    {.name = "lease",
     .kind = WEIVEL_STANDARD_DESCRIBED_POINTER,
     .role = WEIVEL_STANDARD_DESCRIBED_ANSWER,
     .element = WEIVEL_STANDARD_DESCRIBED_STRUCT,
     .type = "probe_lease",
     .count = NULL,
     .release = NULL}};

static const weivel_standard_described_parameter scanned_borrow_parameters[] = {
    {.name = "context",
     .kind = WEIVEL_STANDARD_DESCRIBED_CONTEXT,
     .role = WEIVEL_STANDARD_DESCRIBED_INPUT,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL},
    {.name = "counter",
     .kind = WEIVEL_STANDARD_DESCRIBED_DEFINITION,
     .role = WEIVEL_STANDARD_DESCRIBED_ANSWER,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = "probe_counter",
     .count = NULL,
     .release = NULL}};

static const weivel_standard_described_type scanned_types[] = {
    {.name = "probe_lease",
     .size = sizeof(probe_lease),
     .first_shape = offsetof(probe_lease, remote) + sizeof(((probe_lease *)0)->remote),
     .members = scanned_probe_lease_members,
     .member_count = sizeof(scanned_probe_lease_members) / sizeof(scanned_probe_lease_members[0])},
    {.name = "probe_lender",
     .size = sizeof(probe_lender),
     .first_shape = offsetof(probe_lender, borrow) + sizeof(((probe_lender *)0)->borrow),
     .members = scanned_probe_lender_members,
     .member_count = sizeof(scanned_probe_lender_members) / sizeof(scanned_probe_lender_members[0])}};

static const weivel_standard_described_operation scanned_operations[] = {
    {.name = "lend",
     .offset = offsetof(probe_lender, lend),
     .returns = WEIVEL_STANDARD_DESCRIBED_STATUS,
     .parameters = scanned_lend_parameters,
     .parameter_count = sizeof(scanned_lend_parameters) / sizeof(scanned_lend_parameters[0])},
    {.name = "borrow",
     .offset = offsetof(probe_lender, borrow),
     .returns = WEIVEL_STANDARD_DESCRIBED_STATUS,
     .parameters = scanned_borrow_parameters,
     .parameter_count = sizeof(scanned_borrow_parameters) / sizeof(scanned_borrow_parameters[0])}};

static const char *const scanned_blocking[] = {"lend"};

static const weivel_standard_description *scanned_definitions[] = {&scanned_probe_counter_description, NULL};

static const weivel_standard_description scanned_description = {
    .size = (uint32_t)sizeof(weivel_standard_description),
    .definition = "probe_lender",
    .types = scanned_types,
    .type_count = sizeof(scanned_types) / sizeof(scanned_types[0]),
    .enums = NULL,
    .enum_count = 0,
    .operations = scanned_operations,
    .operation_count = sizeof(scanned_operations) / sizeof(scanned_operations[0]),
    .blocking = scanned_blocking,
    .blocking_count = sizeof(scanned_blocking) / sizeof(scanned_blocking[0]),
    .definitions = scanned_definitions,
    .definition_count = sizeof(scanned_definitions) / sizeof(scanned_definitions[0])};


#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static INIT_ONCE scanned_filled = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK scanned_fill(PINIT_ONCE once, PVOID parameter, PVOID *context) {
    (void)once;
    (void)parameter;
    (void)context;
    scanned_definitions[1] = probe_remote_description();
    return TRUE;
}
#else
#include <pthread.h>

static pthread_once_t scanned_filled = PTHREAD_ONCE_INIT;

static void scanned_fill(void) {
    scanned_definitions[1] = probe_remote_description();
}
#endif

const weivel_standard_description *scanned_description_of(void);

const weivel_standard_description *scanned_description_of(void) {
#if defined(_WIN32)
    InitOnceExecuteOnce(&scanned_filled, scanned_fill, NULL, NULL);
#else
    pthread_once(&scanned_filled, scanned_fill);
#endif
    return &scanned_description;
}
