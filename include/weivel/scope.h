#ifndef WEIVEL_STANDARD_SCOPE_H
#define WEIVEL_STANDARD_SCOPE_H

/**
 * @file
 * The scope service: its id, and its definition, through which a host answers each plugin the standard services as
 * that plugin alone sees them. It follows every convention of `weivel/services.h`.
 *
 * Scope. Each definition a scope answers is the plugin's own, made for that plugin and handed to no other, so the
 * services' rule that no operation names a plugin holds: the plugin is chosen once, when the scope is opened, and
 * never again in a call.
 */

#include <stdint.h>

#include "weivel/core.h"
#include "weivel/services.h"
#include "weivel/standard_api.h"

/** The service id of the scopes a host opens for its plugins. */
#define WEIVEL_STANDARD_SERVICE_SCOPE "weivel.scope"

/** The same id as a function, for a binding that cannot read a macro; it answers a static NUL-terminated string. */
WEIVEL_STANDARD_API const char *weivel_standard_service_scope(void);

/**
 * One plugin's own services, as scopes' `open` answers them: each definition is the plugin's alone, or NULL where the
 * provider offers that plugin none, and each is valid until the scope is released. First shape ends at `storage`.
 */
typedef struct weivel_standard_scope {
    uint32_t size;
    const weivel_standard_config *config;
    const weivel_standard_secrets *secrets;
    const weivel_standard_paths *paths;
    const weivel_standard_log *log;
    void *storage;
} weivel_standard_scope;

/**
 * The scopes a host opens, served under WEIVEL_STANDARD_SERVICE_SCOPE. First shape ends at `release_refusal`.
 */
typedef struct weivel_standard_scopes {
    uint32_t size;
    void *context;
    /**
     * Opens the scope of one plugin.
     *
     * @param manifest_path the path of the plugin's manifest, NUL-terminated UTF-8, naming the plugin. Required.
     * @param scope the consumer's empty answer. Required.
     * @param refusal the consumer's empty refusal, or NULL for none.
     * @return OK with the plugin's own definitions; REFUSED when the provider declines that plugin, as when the
     * settings it declares are wrong.
     */
    weivel_status (*open)(void *context, const char *manifest_path, weivel_standard_scope *scope,
                          weivel_standard_service_refusal *refusal);
    void (*release_scope)(void *context, weivel_standard_scope *scope);
    void (*release_refusal)(void *context, weivel_standard_service_refusal *refusal);
} weivel_standard_scopes;

#endif
