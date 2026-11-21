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
    /** Activates the plugins, ticks, and serves until a stop request. */
    WEIVEL_STANDARD_HOST_SERVE = 1,
    /** Activates the plugins, runs one action, and stops, without ever ticking. */
    WEIVEL_STANDARD_HOST_ACTION = 2
} weivel_standard_host_mode;

/**
 * What became of one plugin the plan named. 0 is no state. The states are append-only: a published state keeps its
 * number, and none is reused.
 */
typedef enum weivel_standard_host_plugin_state {
    WEIVEL_STANDARD_HOST_ACTIVATED = 1,
    /**
     * The plugin was not activated: its scope was refused, or its manifest could not be read or was refused, by the
     * program's standard or its core, the outcome's detail saying why.
     */
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
 * What a run does, as host's `start` answers it. Every text is NUL-terminated UTF-8. `mode` is SERVE or ACTION: any
 * other mode is a malformed answer, which ends the run, the program loading no library, activating no plugin and
 * calling `stop`. The program loads `libraries` in order, then activates `plugins` in order, opening each plugin's
 * scope (`weivel/scope.h`) immediately before that plugin's activation and after the previous plugin's activation
 * ended. The scope is asked of whichever library the program loaded serves a described `weivel.scope`, the library it
 * was started with included; when two of them do, the first loaded wins, the library it was started with first, and a
 * program none of them serves opens no scope and offers the plugin nothing. A program offers no plugin `weivel.host`
 * or `weivel.scope`: it loads and binds a library serving either, and never offers that service where a plugin could
 * resolve it, since a scope is made for one plugin and handed to no other.
 *
 * A library serving `weivel.scope` declares none of `weivel.config`, `weivel.secrets`, `weivel.paths` and
 * `weivel.log`, and offers them through the scope alone. When a loaded library declares one of them and a plugin's
 * scope answers it, that scope cannot be offered: the program offers the plugin none of it, releases the scope, and
 * tells the plugin as REFUSED, the detail saying why.
 *
 * The program reads and checks a plugin's manifest before it opens that plugin's scope: a plugin whose manifest cannot
 * be read, or is refused, gets no scope and is told as REFUSED. In ACTION mode it then runs one action of the plugin
 * whose manifest is `action_plugin`, handing it `action_arguments`, of kind NONE when there are none, and which action
 * is decided by `action_service` and `action_method` alone, a text being set when it is not NULL:
 *
 * - both set: the action is the method `action_method` of the service object the plugin itself registered under the
 *   service id `action_service`, an id its manifest provides, such as `<importName>:accounts`, and `action_name` is
 *   not read. A method is a named operation the object offers, taking `action_arguments` and answering a result or
 *   failing; each binding with a host program names its realisation in its own docs, and one without a host program
 *   has none to realise. A service registered under that id by another plugin, or by none, is one the plugin does
 *   not provide;
 * - neither set: the action is `action_name` among the actions the plugin's entry offers, since a plugin may offer
 *   actions from its entry as well as through the services it provides. A plan from a provider compiled before the
 *   two members reads them as NULL, as the size convention reads what lies beyond the provider's struct;
 * - one set and the other not, or either one empty: the plan is a malformed answer, which ends the run as a malformed
 *   mode does.
 *
 * A provider whose run needs `action_service` and `action_method` and is handed a plan whose `size` cannot hold them,
 * from a program compiled before them, refuses `start`, its detail saying the program is too old for the run.
 *
 * An action whose plugin was not activated, a service that plugin does not provide, or a method or an action it does
 * not offer, is told to `finished` as a failure, and so is a method that throws, as an action that throws is. In
 * SERVE mode every text naming the action is NULL and `action_arguments` is of kind NONE. A library of the plan that
 * will not load ends the run: the program loads no further library, activates no plugin and calls `stop`. First
 * shape ends at `storage`.
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
    /** The service id whose method an ACTION run calls, or NULL. */
    const char *action_service;
    /** The method of `action_service` an ACTION run calls, or NULL. */
    const char *action_method;
} weivel_standard_host_plan;

/** What became of one plugin, as the program tells host's `started`: no size, and it never grows. */
typedef struct weivel_standard_host_outcome {
    /** The path of the plugin's manifest, as the plan named it. */
    const char *manifest;
    /** A weivel_standard_host_plugin_state. */
    uint32_t state;
    /**
     * Why the plugin was refused or failed, as English UTF-8, or NULL for none. It is set, and not empty, whenever the
     * scope's refusal or the activation's failure carried a reason.
     */
    const char *detail;
} weivel_standard_host_outcome;

/**
 * When the program ticks next, as host's `tick` answers it: at `next`, milliseconds since the Unix epoch, when
 * `has_next` is 1. When `has_next` is 0, `tick` is never called again, and the program serves until a stop request.
 * Any other `has_next` is a malformed answer, which ends the run: the program calls `stop` next. First shape ends at
 * `storage`.
 */
typedef struct weivel_standard_host_next {
    uint32_t size;
    uint32_t has_next;
    int64_t next;
    void *storage;
} weivel_standard_host_next;

/**
 * The provider a host program runs, served under WEIVEL_STANDARD_SERVICE_HOST. The program calls `start` once, then
 * opens each plugin's scope and activates the plugin, as the plan says, then calls `started` once with every
 * plugin's outcome. In SERVE mode it then calls `tick` at once and again whenever the time it answered comes, until a
 * stop request. In ACTION mode it never calls `tick`: it runs the action and tells `finished` how it ended. Once
 * `start` has answered OK, the program calls `stop` last, once, whatever happened, including after a REFUSED from
 * `started` or `tick`, after an action, and on a stop request, and exits with the code `stop` answered, which is 0 to
 * 255; when `stop` does not answer OK, the program exits 1. When `start` does not answer OK, the program calls nothing
 * else and exits non-zero. Before it calls `stop`, the program deactivates every plugin it activated and releases
 * every scope it opened; it unloads the libraries after `stop`.
 *
 * A stop request is how a program is asked to end a run: a program takes it through its own interface, and a
 * program's command line takes it from its platform's terminate signals (SIGINT and SIGTERM on POSIX systems, SIGINT
 * and SIGBREAK on Windows). Activation is one phase: one that arrives before `started` is called, including one that
 * arrives before `start` answered OK, which the program keeps, takes effect once `started` has been told the outcome
 * of every plugin of the plan, each one attempted, and the program then calls `stop`, with no `tick` and no action.
 * One that arrives during a later call, as during a blocking `tick`, takes effect once that call returns: the program
 * calls `stop` next, and no further `tick`. One that arrives while an ACTION run's action runs takes effect once
 * `finished` has been told how it ended. First shape ends at `release_refusal`.
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
     * @return OK with the plan; REFUSED when the arguments name no run the provider can make, or the run needs a
     * member the plan's `size` cannot hold.
     */
    weivel_status (*start)(void *context, const char *const *arguments, size_t argument_count,
                           weivel_standard_host_plan *plan, weivel_standard_service_refusal *refusal);
    /**
     * Tells what became of the plan's plugins.
     *
     * @param outcomes one outcome per plugin, in the plan's order, read during the call only; NULL only when
     * `outcome_count` is 0.
     * @param refusal the consumer's empty refusal, or NULL for none.
     * @return OK; REFUSED when the provider will not run on with these outcomes, and the program calls `stop`.
     */
    weivel_status (*started)(void *context, const weivel_standard_host_outcome *outcomes, size_t outcome_count,
                             weivel_standard_service_refusal *refusal);
    /**
     * Lets the provider act on the time, and asks when to tick next. Called in SERVE mode only: at once after
     * `started` answered OK, then whenever the time the previous call answered comes, the program reading its clock
     * again before each call and waiting again while it still reads before `next`. @blocking the provider may check
     * for updates, which waits on I/O.
     *
     * @param now milliseconds since the Unix epoch, as the program's clock reads.
     * @param next the consumer's empty answer. Required.
     * @param refusal the consumer's empty refusal, or NULL for none.
     * @return OK; REFUSED when the provider cannot act, and the program calls `stop`.
     */
    weivel_status (*tick)(void *context, int64_t now, weivel_standard_host_next *next,
                          weivel_standard_service_refusal *refusal);
    /**
     * Tells how an ACTION run ended, once, after the action ended.
     *
     * @param result what the action answered, read during the call only, of kind NONE when it answered nothing,
     * and NULL only when it threw.
     * @param failure why the action threw, NUL-terminated UTF-8, or NULL when it answered.
     * @param refusal the consumer's empty refusal, or NULL for none.
     * @return OK; MALFORMED when both or neither of `result` and `failure` are set.
     */
    weivel_status (*finished)(void *context, const weivel_standard_value *result, const char *failure,
                              weivel_standard_service_refusal *refusal);
    /**
     * Ends the run.
     *
     * @param exit_code the code the program exits with, 0 to 255. Required.
     * @param refusal the consumer's empty refusal, or NULL for none.
     * @return OK; REFUSED when the provider could not stop cleanly.
     */
    weivel_status (*stop)(void *context, uint32_t *exit_code, weivel_standard_service_refusal *refusal);
    void (*release_plan)(void *context, weivel_standard_host_plan *plan);
    void (*release_next)(void *context, weivel_standard_host_next *next);
    void (*release_refusal)(void *context, weivel_standard_service_refusal *refusal);
} weivel_standard_host;

#endif
