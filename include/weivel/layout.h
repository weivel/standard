#ifndef WEIVEL_STANDARD_LAYOUT_H
#define WEIVEL_STANDARD_LAYOUT_H

#include <stddef.h>
#include <stdint.h>

#include "weivel/core.h"
#include "weivel/identity.h"
#include "weivel/standard.h"
#include "weivel/standard_api.h"

/**
 * One plugin found laid out by the standard: `<importName>.json`, beside it an optional
 * `<importName>_identity.json` holding `{"id": "<identity>", "name": "<repository name>"}`, and the file its
 * manifest's `entry` names, resolved against the same directory. Every path is UTF-8, the directory as the caller
 * gave it joined to a name by `/`. Everything lives until weivel_standard_discovery_release.
 */
typedef struct weivel_standard_discovered_plugin {
    /** The manifest, loaded by weivel_standard_load_manifest and so validated; owned by the discovery. */
    const weivel_standard_manifest *manifest;
    const char *manifest_path;
    /** NULL, with `identity_id`, `repository_name` and an empty `identity`, when no identity file sits beside it. */
    const char *identity_path;
    /** The identity file's `id`, for a host to set as the manifest's identity. */
    const char *identity_id;
    const char *repository_name;
    /** `identity_id` taken apart. */
    weivel_standard_identity identity;
    /** The manifest's `entry` joined to the directory; NULL when the manifest declares none. */
    const char *entry_path;
    /** 1 when `entry_path` names a regular file, a link not being followed to find one, else 0. */
    uint32_t entry_present;
} weivel_standard_discovered_plugin;

/** A file discovery could not take a plugin from, and why: `file` is its path. */
typedef struct weivel_standard_discovery_failure {
    const char *file;
    weivel_standard_refusal refusal;
} weivel_standard_discovery_failure;

/** What one scan of a directory found. Everything lives until weivel_standard_discovery_release. */
typedef struct weivel_standard_discovery {
    uint32_t size;
    const weivel_standard_discovered_plugin *plugins;
    size_t plugin_count;
    const weivel_standard_discovery_failure *failures;
    size_t failure_count;
    /** Why the directory itself could not be scanned, on WEIVEL_STATUS_REFUSED and WEIVEL_STATUS_MALFORMED. */
    weivel_standard_refusal refusal;
} weivel_standard_discovery;

/**
 * Scans one directory, not its subdirectories, for plugins laid out by the standard. A regular file named
 * `<name>.json`, where `<name>` is a valid importName, is a manifest; every other entry is ignored, among them each
 * `*_identity.json` (an importName holds no underscore), a `package.json` a language layer may write (no importName is
 * `package`), a directory, and a link, which is never followed.
 *
 * Every manifest that loads is a plugin, and every one that does not is a failure naming its file, so one bad file
 * never hides the rest: a file that cannot be read (UNREADABLE), is not JSON (MALFORMED_JSON) or does not validate
 * (its first refusal, with its field `path`), a manifest whose importName is not its file name (NAME_MISMATCH), an
 * identity file that is not a regular file, not JSON, or lacks a string `id` that is an identity or a non-empty
 * string `name` (failing at that field's path), and a plugin whose identity an earlier one in order already holds
 * (DUPLICATE, naming its identity file). Failures come in the order of their file names, repeated identities last.
 *
 * Plugins come ordered by identity, by weivel_standard_identity_compare, and then every plugin with no identity file
 * after all of those, by importName. A missing identity file is not a failure: the plugin is local and it is the
 * host's to give it an identity or none.
 *
 * @param directory the directory, UTF-8. Required, and not "".
 * @param options passed to weivel_standard_load_manifest for every manifest; NULL for every default.
 * @param discovery the caller's struct with `size` set and nothing held, as WEIVEL_INIT or a release leaves it.
 * @return WEIVEL_STATUS_OK when the directory was read, whatever its files held: a directory with no plugin in it
 * answers OK with nothing found. WEIVEL_STATUS_REFUSED when it could not be: NOT_FOUND for a directory that does not
 * exist, which a host that treats a missing directory as an empty one maps to nothing found itself, and UNREADABLE
 * for one that cannot be listed or is not a directory. WEIVEL_STATUS_MALFORMED for a NULL or empty directory or a
 * NULL discovery, a size below its first shape, or a discovery still holding an answer, which is left as it was;
 * WEIVEL_STATUS_OUT_OF_MEMORY, with the discovery left empty.
 */
WEIVEL_STANDARD_API weivel_status weivel_standard_discover(const char *directory,
                                                           const weivel_standard_validation_options *options,
                                                           weivel_standard_discovery *discovery);

/** Frees what a discovery holds and leaves it empty with its `size` kept; a no-op for NULL or an empty one. */
WEIVEL_STANDARD_API void weivel_standard_discovery_release(weivel_standard_discovery *discovery);

#endif
