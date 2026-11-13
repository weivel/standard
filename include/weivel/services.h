#ifndef WEIVEL_STANDARD_SERVICES_H
#define WEIVEL_STANDARD_SERVICES_H

/**
 * @file
 * The config, secrets and paths services: their ids, and each one's definition as a C struct of function pointers,
 * the one definition every language translates. A provider fills the struct and serves it as its service object; a
 * consumer calls through it. No value crossing these definitions is JSON.
 *
 * Conventions every definition here follows, beside `weivel/standard.h`'s:
 *
 * - Size. Every size-prefixed struct here has a first shape: the struct as this header publishes it now, ending at
 *   the field each struct's doc names. Fields and operations are only ever appended. A struct whose `size` reaches at
 *   least its first shape is read up to its `size`, what lies beyond taking its default; one below it is malformed.
 *   The provider sets a definition's `size` to `sizeof` the struct as the provider was compiled, and the consumer
 *   checks with WEIVEL_STANDARD_OFFERS before calling an operation appended after the first shape. An answer or a
 *   refusal the consumer hands in carries the consumer's `size`: the provider writes only within it, leaves `size` as
 *   the consumer set it, and zeroes whatever a newer consumer has beyond the provider's own struct.
 * - Context. Every operation receives the definition's own `context` first, opaque to everyone but the provider.
 * - Answers. Every operation answers a weivel_status: WEIVEL_STATUS_OK; WEIVEL_STATUS_NONE where an operation says
 *   there was nothing to answer; WEIVEL_STATUS_MALFORMED for a NULL required pointer, an answer or refusal below its
 *   first shape or still holding storage, or a value the operation's doc does not allow, with nothing changed and
 *   nothing filled; WEIVEL_STATUS_REFUSED when the provider declines, its refusal filled when one was handed in;
 *   WEIVEL_STATUS_OUT_OF_MEMORY, with nothing changed. An answer is filled on OK alone.
 * - Memory. Text and bytes the consumer hands in are borrowed for the call only. What a filled answer or refusal
 *   points at is the provider's until the consumer hands that struct to the definition's release for its kind, which
 *   frees it and leaves the struct empty with its `size` kept, as weivel_standard_refusal_release does; a release is a
 *   no-op for NULL, an empty struct or one below its first shape. Each kind of struct has its own release, so a
 *   provider never has to tell an answer's storage from a refusal's.
 * - Threads. A resolved service is the provider's own object, so every plugin that resolves one holds the same
 *   definition and none can lock against the others. A provider is therefore safe for concurrent calls from any
 *   thread, its releases included, and a provider that is not takes its own lock. An answer outlives every later call
 *   until it is released.
 * - Time. Every operation completes before it returns. A language whose interface is asynchronous presents a call as
 *   asynchronous; a provider in another process answers once the write has reached it.
 * - Scope. No operation names a plugin, and one definition cannot tell its callers apart. A host that wants each
 *   plugin to see its own settings, secrets or directories does it by what it offers each plugin, outside these
 *   definitions.
 */

#include <stddef.h>
#include <stdint.h>

#include "weivel/core.h"
#include "weivel/standard_api.h"

/** The service id of the settings a host reads. */
#define WEIVEL_STANDARD_SERVICE_CONFIG "weivel.config"
/** The service id of the secrets a host keeps. */
#define WEIVEL_STANDARD_SERVICE_SECRETS "weivel.secrets"
/** The service id of the places a host stores files. */
#define WEIVEL_STANDARD_SERVICE_PATHS "weivel.paths"

/** The same ids as functions, for a binding that cannot read a macro; each answers a static NUL-terminated string. */
WEIVEL_STANDARD_API const char *weivel_standard_service_config(void);
WEIVEL_STANDARD_API const char *weivel_standard_service_secrets(void);
WEIVEL_STANDARD_API const char *weivel_standard_service_paths(void);

/**
 * Whether `definition`, a pointer to a `type` below, carries `operation`: the provider's `size` reaches past it and
 * the pointer is set. The operation is read only once the size is known to cover it.
 */
#define WEIVEL_STANDARD_OFFERS(type, definition, operation)                              \
    ((definition)->size >= offsetof(type, operation) + sizeof(((type *)0)->operation) && \
     (definition)->operation != NULL)

/**
 * Why a provider declined, as English UTF-8 the provider owns: `detail` says what happened and `fix` what would make
 * the call succeed. An empty refusal has its `size` set and NULL texts and storage, as WEIVEL_INIT leaves it. Its
 * first shape ends at `storage`.
 */
typedef struct weivel_standard_service_refusal {
    uint32_t size;
    const char *detail;
    const char *fix;
    void *storage;
} weivel_standard_service_refusal;

/** What a setting holds. The kinds are append-only: a published kind keeps its number, and none is reused. */
typedef enum weivel_standard_setting_kind {
    /** No value. An answer that is not filled holds it, and it is never a value to store. */
    WEIVEL_STANDARD_SETTING_NONE = 0,
    WEIVEL_STANDARD_SETTING_TEXT = 1,
    WEIVEL_STANDARD_SETTING_NUMBER = 2,
    WEIVEL_STANDARD_SETTING_BOOLEAN = 3
} weivel_standard_setting_kind;

/**
 * One setting's value: `kind` says which field holds it, and every other field is zero. `text` is UTF-8 of
 * `text_length` bytes followed by a NUL; `number` a finite IEEE 754 double, so an integer is exact only up to 2^53 in
 * magnitude; `boolean` 0 or 1. A value sits inside answers and array elements, so it carries no size and never grows:
 * a later kind whose value none of these fields can hold arrives as a new definition.
 */
typedef struct weivel_standard_setting_value {
    uint32_t kind;
    uint32_t boolean;
    double number;
    const char *text;
    size_t text_length;
} weivel_standard_setting_value;

/** One named setting, an element of weivel_standard_settings: no size, and it never grows. */
typedef struct weivel_standard_setting {
    /** NUL-terminated UTF-8. */
    const char *key;
    weivel_standard_setting_value value;
} weivel_standard_setting;

/** Every setting, as config's `all` answers them: each key once, in no promised order. First shape ends at `storage`. */
typedef struct weivel_standard_settings {
    uint32_t size;
    const weivel_standard_setting *settings;
    size_t count;
    void *storage;
} weivel_standard_settings;

/** One setting's value, as config's `get` answers it. First shape ends at `storage`. */
typedef struct weivel_standard_setting_answer {
    uint32_t size;
    weivel_standard_setting_value value;
    void *storage;
} weivel_standard_setting_answer;

/**
 * The settings a host reads, served under WEIVEL_STANDARD_SERVICE_CONFIG. A key is NUL-terminated UTF-8. First shape
 * ends at `release_refusal`.
 */
typedef struct weivel_standard_config {
    uint32_t size;
    void *context;
    /**
     * Every setting: the provider's defaults merged with what the host stores, a stored value winning.
     *
     * @param settings the consumer's empty answer. Required.
     * @param refusal the consumer's empty refusal, or NULL for none.
     * @return OK, `count` 0 when there are none; REFUSED when the settings cannot be read.
     */
    weivel_status (*all)(void *context, weivel_standard_settings *settings, weivel_standard_service_refusal *refusal);
    /**
     * One setting.
     *
     * @param key Required.
     * @param answer the consumer's empty answer. Required.
     * @param refusal the consumer's empty refusal, or NULL for none.
     * @return OK; NONE when the key is neither stored nor defaulted; REFUSED when the settings cannot be read.
     */
    weivel_status (*get)(void *context, const char *key, weivel_standard_setting_answer *answer,
                         weivel_standard_service_refusal *refusal);
    /**
     * Stores one setting, replacing any value it had, so `get` and `all` answer it from then on.
     *
     * @param key Required.
     * @param value read during the call only. Required.
     * @param refusal the consumer's empty refusal, or NULL for none.
     * @return OK; MALFORMED for a value of kind NONE or a kind the provider does not know, a NULL `text`, a number that
     * is not finite, or a boolean other than 0 or 1, since none of them is a value; REFUSED when the provider will not
     * store the key or the value, or cannot write.
     */
    weivel_status (*set)(void *context, const char *key, const weivel_standard_setting_value *value,
                         weivel_standard_service_refusal *refusal);
    void (*release_settings)(void *context, weivel_standard_settings *settings);
    void (*release_answer)(void *context, weivel_standard_setting_answer *answer);
    void (*release_refusal)(void *context, weivel_standard_service_refusal *refusal);
} weivel_standard_config;

/**
 * One secret, as secrets' `get` answers it: `length` bytes, any bytes at all, so a language whose interface takes
 * text stores it as UTF-8. A provider should overwrite the bytes before it frees them. First shape ends at `storage`.
 */
typedef struct weivel_standard_secret {
    uint32_t size;
    const uint8_t *bytes;
    size_t length;
    void *storage;
} weivel_standard_secret;

/**
 * The secrets a host keeps, served under WEIVEL_STANDARD_SERVICE_SECRETS. A name is NUL-terminated UTF-8 and
 * required; a refusal is the consumer's empty one, or NULL for none. First shape ends at `release_refusal`.
 *
 * @implNote `remove` and not `delete`, so the header stays includable from C++, where `delete` is a keyword.
 */
typedef struct weivel_standard_secrets {
    uint32_t size;
    void *context;
    /**
     * Removes one secret.
     *
     * @return OK, also when the name holds no secret; REFUSED when the secrets cannot be written.
     */
    weivel_status (*remove)(void *context, const char *name, weivel_standard_service_refusal *refusal);
    /**
     * One secret.
     *
     * @param secret the consumer's empty answer. Required.
     * @return OK; NONE when the name holds no secret; REFUSED when the secrets cannot be read.
     */
    weivel_status (*get)(void *context, const char *name, weivel_standard_secret *secret,
                         weivel_standard_service_refusal *refusal);
    /**
     * Stores one secret, replacing any value it had.
     *
     * @param bytes `length` bytes, read during the call only; NULL only when `length` is 0.
     * @return OK; REFUSED when the provider will not store it or cannot write.
     */
    weivel_status (*set)(void *context, const char *name, const uint8_t *bytes, size_t length,
                         weivel_standard_service_refusal *refusal);
    void (*release_secret)(void *context, weivel_standard_secret *secret);
    void (*release_refusal)(void *context, weivel_standard_service_refusal *refusal);
} weivel_standard_secrets;

/**
 * The absolute directories a host stores files in, served under WEIVEL_STANDARD_SERVICE_PATHS. Each operation sets
 * `*directory` to a NUL-terminated UTF-8 absolute path the provider owns, the same on every call and valid while the
 * definition is, so nothing is released; it answers OK, or MALFORMED for a NULL `directory`. Whether the directory
 * exists yet is not promised. First shape ends at `plugin`.
 */
typedef struct weivel_standard_paths {
    uint32_t size;
    void *context;
    /** Where files that can be rebuilt are kept. */
    weivel_status (*cache)(void *context, const char **directory);
    /** Where settings files are kept. */
    weivel_status (*config)(void *context, const char **directory);
    /** The directory the host offers as the plugin's own. */
    weivel_status (*plugin)(void *context, const char **directory);
} weivel_standard_paths;

#endif
