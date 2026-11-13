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
    WEIVEL_STANDARD_REFUSAL_UNREPRESENTABLE = 4
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
 * Reads a JSON manifest into the core's weivel_manifest. It converts and does not validate: a field the core's
 * manifest carries is refused only when its JSON type is wrong or its value cannot be held, a field left out takes
 * the core's default (a required one such as `importName` reads as NULL, which the core refuses on registration),
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

#endif
