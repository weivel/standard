#ifndef WEIVEL_STANDARD_HOST_H
#define WEIVEL_STANDARD_HOST_H

/**
 * @file
 * The host service: its id, and its definition, through which a host program asks its provider what to run and tells
 * it what happened. It follows every convention of `weivel/services.h`.
 *
 * `start` decides everything the program does: which libraries it loads, which plugins it activates and in which
 * order, and whether it serves or runs one action. The program decides nothing of its own. Time reaches C only
 * through `tick`'s `now`.
 */

#include <stddef.h>
#include <stdint.h>

#include "weivel/core.h"
#include "weivel/services.h"
#include "weivel/standard_api.h"
#include "weivel/value.h"

/** The service id of the provider a host program runs. */
#define WEIVEL_STANDARD_SERVICE_HOST "weivel.host"

/** The same id as a function, for a binding that cannot read a macro; it answers a static NUL-terminated string. */
WEIVEL_STANDARD_API const char *weivel_standard_service_host(void);

/** What a run does. 0 is no mode. The modes are append-only: a published mode keeps its number, and none is reused. */
typedef enum weivel_standard_host_mode {
    /** Activates the plugins and serves until stopped. */
    WEIVEL_STANDARD_HOST_SERVE = 1,
    /** Activates the plugins, runs one action, and stops. */
    WEIVEL_STANDARD_HOST_ACTION = 2
} weivel_standard_host_mode;

/**
 * What became of one plugin the plan named. 0 is no state. The states are append-only: a published state keeps its
 * number, and none is reused.
 */
typedef enum weivel_standard_host_plugin_state {
    WEIVEL_STANDARD_HOST_ACTIVATED = 1,
    /** The plugin was not activated, as when its scope was refused. */
    WEIVEL_STANDARD_HOST_REFUSED = 2,
    /** The plugin's activation threw. */
    WEIVEL_STANDARD_HOST_FAILED = 3
} weivel_standard_host_plugin_state;

/** One library the program loads, an element of the plan: no size, and it never grows. */
typedef struct weivel_standard_host_library {
    /** The path of the library's binary. */
    const char *binary;
    /** The path of the library's declaration, its `library.json`. */
    const char *declaration;
} weivel_standard_host_library;

/** One plugin the program activates, an element of the plan: no size, and it never grows. */
typedef struct weivel_standard_host_plugin {
    /** The plugin's directory. */
    const char *directory;
    /** The path of the plugin's manifest. */
    const char *manifest;
    /** The path of the plugin's entry. */
    const char *entry;
} weivel_standard_host_plugin;

/**
 * What a run does, as host's `start` answers it. Every text is NUL-terminated UTF-8. The program loads `libraries` in
 * order, then activates `plugins` in order. In ACTION mode it then runs the action `action_name` of the plugin whose
 * manifest is `action_plugin`, with `action_arguments`, of kind NONE when there are none; in SERVE mode both texts are
 * NULL and `action_arguments` is of kind NONE. First shape ends at `storage`.
 */
typedef struct weivel_standard_host_plan {
    uint32_t size;
    /** A weivel_standard_host_mode. */
    uint32_t mode;
    const weivel_standard_host_library *libraries;
    size_t library_count;
    const weivel_standard_host_plugin *plugins;
    size_t plugin_count;
    const char *action_plugin;
    const char *action_name;
    weivel_standard_value action_arguments;
    void *storage;
} weivel_standard_host_plan;

/** What became of one plugin, as the program tells host's `started`: no size, and it never grows. */
typedef struct weivel_standard_host_outcome {
    /** The path of the plugin's manifest, as the plan named it. */
    const char *manifest;
    /** A weivel_standard_host_plugin_state. */
    uint32_t state;
    /** Why the plugin was refused or failed, as English UTF-8, or NULL for none. */
    const char *detail;
} weivel_standard_host_outcome;

/**
 * When the program ticks next, as host's `tick` answers it: at `next`, milliseconds since the Unix epoch, when
 * `has_next` is 1, and only when something else wakes it when `has_next` is 0. First shape ends at `storage`.
 */
typedef struct weivel_standard_host_next {
    uint32_t size;
    uint32_t has_next;
    int64_t next;
    void *storage;
} weivel_standard_host_next;

/**
 * The provider a host program runs, served under WEIVEL_STANDARD_SERVICE_HOST. The program calls `start` once, then
 * `started`, then `tick` whenever the time it answered comes, `finished` after an ACTION run, and `stop` last. First
 * shape ends at `release_refusal`.
 */
typedef struct weivel_standard_host {
    uint32_t size;
    void *context;
    /**
     * Decides the run.
     *
     * @param arguments the program's own arguments, each NUL-terminated UTF-8, read during the call only; NULL only
     * when `argument_count` is 0.
     * @param plan the consumer's empty answer. Required.
     * @param refusal the consumer's empty refusal, or NULL for none.
     * @return OK with the plan; REFUSED when the arguments name no run the provider can make.
     */
    weivel_status (*start)(void *context, const char *const *arguments, size_t argument_count,
                           weivel_standard_host_plan *plan, weivel_standard_service_refusal *refusal);
    /**
     * Tells what became of the plan's plugins.
     *
     * @param outcomes one outcome per plugin, in the plan's order, read during the call only; NULL only when
     * `outcome_count` is 0.
     * @param refusal the consumer's empty refusal, or NULL for none.
     * @return OK; REFUSED when the provider will not run on with these outcomes, and the program stops.
     */
    weivel_status (*started)(void *context, const weivel_standard_host_outcome *outcomes, size_t outcome_count,
                             weivel_standard_service_refusal *refusal);
    /**
     * Lets the provider act on the time, and asks when to tick next. @blocking the provider may check for updates,
     * which waits on I/O.
     *
     * @param now milliseconds since the Unix epoch, as the program's clock reads.
     * @param next the consumer's empty answer. Required.
     * @param refusal the consumer's empty refusal, or NULL for none.
     * @return OK; REFUSED when the provider cannot act, and the program stops.
     */
    weivel_status (*tick)(void *context, int64_t now, weivel_standard_host_next *next,
                          weivel_standard_service_refusal *refusal);
    /**
     * Tells how an ACTION run ended.
     *
     * @param result what the action answered, read during the call only, or NULL when it threw.
     * @param failure why the action threw, NUL-terminated UTF-8, or NULL when it answered.
     * @param refusal the consumer's empty refusal, or NULL for none.
     * @return OK; MALFORMED when both or neither of `result` and `failure` are set.
     */
    weivel_status (*finished)(void *context, const weivel_standard_value *result, const char *failure,
                              weivel_standard_service_refusal *refusal);
    /**
     * Ends the run.
     *
     * @param exit_code the code the program exits with. Required.
     * @param refusal the consumer's empty refusal, or NULL for none.
     * @return OK; REFUSED when the provider could not stop cleanly.
     */
    weivel_status (*stop)(void *context, uint32_t *exit_code, weivel_standard_service_refusal *refusal);
    void (*release_plan)(void *context, weivel_standard_host_plan *plan);
    void (*release_next)(void *context, weivel_standard_host_next *next);
    void (*release_refusal)(void *context, weivel_standard_service_refusal *refusal);
} weivel_standard_host;

#endif
