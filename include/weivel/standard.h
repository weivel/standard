#ifndef WEIVEL_STANDARD_H
#define WEIVEL_STANDARD_H

#include <stddef.h>
#include <stdint.h>

#include "weivel/core.h"
#include "weivel/standard_api.h"

/** Why the standard refused something. */
typedef enum weivel_standard_refusal_code {
    WEIVEL_STANDARD_REFUSAL_NONE = 0,
    /** A call was malformed (a NULL required pointer, a refusal still holding one); detail says which. */
    WEIVEL_STANDARD_REFUSAL_MALFORMED_CALL = 1,
    /** The text is not one well-formed JSON document; detail gives the line and column. */
    WEIVEL_STANDARD_REFUSAL_MALFORMED_JSON = 2,
    /** A field the core's manifest carries holds a JSON value of the wrong type; path names it. */
    WEIVEL_STANDARD_REFUSAL_WRONG_TYPE = 3,
    /**
     * A field has the right type and a value the core's manifest cannot hold: an api outside 0 to 4294967295, a
     * string holding a NUL character, a list longer than 4294967295 items; path names it.
     */
    WEIVEL_STANDARD_REFUSAL_UNREPRESENTABLE = 4,
    /** A field the schema requires is absent; path names the field that is missing. */
    WEIVEL_STANDARD_REFUSAL_MISSING_FIELD = 5,
    /** A field has the right type and a value its rule refuses (a pattern, a minimum, a length); path names it. */
    WEIVEL_STANDARD_REFUSAL_INVALID_VALUE = 6,
    /** A list item repeats an earlier one (V1, or `uniqueItems`); path names the later occurrence. */
    WEIVEL_STANDARD_REFUSAL_DUPLICATE = 7,
    /** A provided service id is neither namespaced by the plugin's importName nor well-known to the host (V2). */
    WEIVEL_STANDARD_REFUSAL_NOT_WELL_KNOWN = 8,
    /** A field is present beside one that excludes it, as a dependency's `name` beside its `id`; path names it. */
    WEIVEL_STANDARD_REFUSAL_CONFLICTING_FIELD = 9
} weivel_standard_refusal_code;

/**
 * A refusal, as a value, shaped like the core's. `detail` says what happened and `fix` what would make it succeed,
 * both English UTF-8; `path` names the field, as `services.consumes[1].provider`, and is empty for the whole
 * document. Everything lives in `storage`, private to the standard, until weivel_standard_refusal_release. An empty
 * refusal is code NONE with NULL texts and storage, as a zeroed struct or a release leaves it.
 */
typedef struct weivel_standard_refusal {
    uint32_t code;
    const char *detail;
    const char *fix;
    const char *path;
    void *storage;
} weivel_standard_refusal;

/** Frees what a refusal carries and leaves it empty; a no-op for NULL or an empty refusal. */
WEIVEL_STANDARD_API void weivel_standard_refusal_release(weivel_standard_refusal *refusal);

/** A manifest read from JSON. Opaque; owned by the caller until weivel_standard_manifest_release. */
typedef struct weivel_standard_manifest weivel_standard_manifest;

/**
 * Reads a JSON manifest into the core's weivel_manifest. It converts and DOES NOT VALIDATE: a host taking the
 * standard's defaults calls weivel_standard_load_manifest, which validates first. A field the core's manifest carries
 * is refused here only when its JSON type is wrong or its value cannot be held, a field left out takes the core's
 * default (a required one such as `importName` reads as NULL, which validation refuses with its path and the core
 * refuses on registration without one),
 * and every field the core's manifest has no place for (`dependencies` among them) is ignored. `null` is a value of
 * the wrong type, never an absent field. The manifest's `identity` is left NULL: it is supplied by whatever loaded
 * the plugin, not written in its manifest.
 *
 * @param json the document, read only during the call; it need not be NUL-terminated, and the caller may free it as
 * soon as the call returns. Required.
 * @param manifest receives the manifest on WEIVEL_STATUS_OK and NULL on every other answer. Required.
 * @param refusal the caller's empty refusal, or NULL for none; filled on WEIVEL_STATUS_REFUSED and
 * WEIVEL_STATUS_MALFORMED, and left empty on every other answer.
 * @return WEIVEL_STATUS_OK; WEIVEL_STATUS_REFUSED for malformed JSON, a wrong type or an unrepresentable value;
 * WEIVEL_STATUS_MALFORMED for a NULL json or manifest, or a refusal still holding one, which is left as it was;
 * WEIVEL_STATUS_OUT_OF_MEMORY, with nothing allocated.
 */
WEIVEL_STANDARD_API weivel_status weivel_standard_read_manifest(const char *json, size_t length,
                                                                weivel_standard_manifest **manifest,
                                                                weivel_standard_refusal *refusal);

/**
 * The manifest as the core takes it, ready for weivel_supports and weivel_register_manifest. Everything it points at
 * lives until weivel_standard_manifest_release. A host that supplies an identity copies the struct and sets
 * `identity` on its copy. NULL for a NULL manifest.
 */
WEIVEL_STANDARD_API const weivel_manifest *weivel_standard_manifest_view(const weivel_standard_manifest *manifest);

/** Frees a manifest and everything its view points at; a no-op for NULL. */
WEIVEL_STANDARD_API void weivel_standard_manifest_release(weivel_standard_manifest *manifest);

/** Options for weivel_standard_validate. Zero, or a field beyond `size`, means the default. */
typedef struct weivel_standard_validation_options {
    uint32_t size;
    /**
     * The host's well-known service ids, for rule V2: a provided id is legal when it is one of them or is namespaced
     * as `<importName>:<name>`. NULL, the default, means V2 is not applied; a list of no ids refuses every bare id.
     */
    const weivel_strings *well_known_services;
} weivel_standard_validation_options;

/**
 * What weivel_standard_validate found. `paths` names every field that is wrong, each once, in the order the
 * validator reached them; a repeated item is named at the index of its later occurrence. `refusal` is the first of
 * them as data, and its `path` is `paths.items[0]`. An empty manifest path ("") is the whole document. Everything
 * lives in `storage`, private to the standard, until weivel_standard_validation_release.
 */
typedef struct weivel_standard_validation {
    uint32_t size;
    /** 1 when the manifest is valid and `paths` is empty, else 0. */
    uint32_t valid;
    weivel_strings paths;
    weivel_standard_refusal refusal;
    void *storage;
} weivel_standard_validation;

/**
 * Validates a JSON manifest by the standard's schema, `schema/plugin.schema.json`, and the rules VALIDATION.md adds
 * to it: V1 (a service is listed at most once on each side, its string and object forms naming the same service) and
 * V2 (a provided id is namespaced or well-known) when `options` carries a well-known list. A field the schema does
 * not name validates and is ignored, at any depth.
 *
 * @param json the document, read only during the call; it need not be NUL-terminated. Required.
 * @param options NULL for every default.
 * @param validation the caller's struct with `size` set and nothing held, as WEIVEL_INIT or a release leaves it.
 * Filled on WEIVEL_STATUS_OK and WEIVEL_STATUS_REFUSED, and with a MALFORMED_CALL refusal on WEIVEL_STATUS_MALFORMED
 * when it is itself usable.
 * @return WEIVEL_STATUS_OK for a valid manifest; WEIVEL_STATUS_REFUSED for an invalid one or malformed JSON (a
 * MALFORMED_JSON refusal at path ""); WEIVEL_STATUS_MALFORMED for a NULL json or validation, a size below the first
 * shape of either struct, or a validation still holding an answer, which is left as it was;
 * WEIVEL_STATUS_OUT_OF_MEMORY, with the validation left empty.
 */
WEIVEL_STANDARD_API weivel_status weivel_standard_validate(const char *json, size_t length,
                                                           const weivel_standard_validation_options *options,
                                                           weivel_standard_validation *validation);

/** Frees what a validation carries and leaves it empty with its `size` kept; a no-op for NULL or an empty one. */
WEIVEL_STANDARD_API void weivel_standard_validation_release(weivel_standard_validation *validation);

/**
 * Validates a JSON manifest and, only when it is valid, reads it, so a host that takes the standard's defaults cannot
 * hand the core a manifest that skipped validation. A value the schema allows and the core's manifest cannot hold
 * (an api above 4294967295) is answered as the reader refuses it, with `paths` naming its one field.
 *
 * @param manifest receives the manifest on WEIVEL_STATUS_OK and NULL on every other answer. Required.
 * @return as weivel_standard_validate, with `manifest` set only on WEIVEL_STATUS_OK.
 */
WEIVEL_STANDARD_API weivel_status weivel_standard_load_manifest(const char *json, size_t length,
                                                                const weivel_standard_validation_options *options,
                                                                weivel_standard_manifest **manifest,
                                                                weivel_standard_validation *validation);

#endif
