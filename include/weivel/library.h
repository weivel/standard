#ifndef WEIVEL_STANDARD_LIBRARY_H
#define WEIVEL_STANDARD_LIBRARY_H

#include <stddef.h>
#include <stdint.h>

#include "weivel/core.h"
#include "weivel/json.h"
#include "weivel/standard.h"
#include "weivel/standard_api.h"

/** How a library function is called. */
typedef enum weivel_standard_function_kind {
    /** A JSON value in and a JSON value out, each checked against the function's `argument` and `result` schema. */
    WEIVEL_STANDARD_FUNCTION_JSON = 1,
    /**
     * Opaque bytes in and opaque bytes out, as
     * `int32_t f(const uint8_t *input, size_t input_length, uint8_t **output, size_t *output_length)`. Status 0
     * answers the result; any other status refuses, its body, when there is one, English UTF-8 saying why. Every
     * body is the library's, freed by its `release` symbol as `void release(uint8_t *buffer)`. The declaration
     * carries no `argument` or `result`, since neither has a shape the standard could check.
     */
    WEIVEL_STANDARD_FUNCTION_BYTES = 2
} weivel_standard_function_kind;

/** The facts of the calling process a library function reads, as bits of its `context`. */
typedef enum weivel_standard_context_fact {
    WEIVEL_STANDARD_CONTEXT_ENVIRONMENT = 1,
    WEIVEL_STANDARD_CONTEXT_HOME = 2,
    WEIVEL_STANDARD_CONTEXT_ARGUMENTS = 4
} weivel_standard_context_fact;

typedef struct weivel_standard_library_function {
    /** The name a plugin calls the function by. */
    const char *name;
    /** The exported C symbol that implements it. */
    const char *symbol;
    /** A weivel_standard_function_kind. */
    uint32_t kind;
    /** The facts the function reads, as weivel_standard_context_fact bits; 0 when it reads none. */
    uint32_t context;
    /** The JSON Schemas of its argument and its result, each an object or a boolean; NULL for a bytes function. */
    const weivel_json_value *argument;
    const weivel_json_value *result;
} weivel_standard_library_function;

typedef struct weivel_standard_library_service {
    /** The service id plugins request. */
    const char *id;
    const weivel_standard_library_function *functions;
    size_t function_count;
} weivel_standard_library_service;

/**
 * A C library's declaration: the name plugins import it by, its version, the symbol that frees what it returns, and
 * the services its functions offer. Everything it points at lives in `storage`, private to the standard, until
 * weivel_standard_library_release. An empty declaration has its `size` set and every other field zero, as WEIVEL_INIT
 * or a release leaves it.
 */
typedef struct weivel_standard_library {
    uint32_t size;
    const char *import_name;
    const char *version;
    const char *release;
    const weivel_standard_library_service *services;
    size_t service_count;
    void *storage;
} weivel_standard_library;

/**
 * Reads a library declaration and checks it by the declaration's rules, answering the first field that breaks one.
 * A field the format does not define is ignored at every level, so a declaration written for a later version still
 * loads; `$schema` is one of them, as where a schema is published is no part of what a library is. How a language
 * binds the functions is the language layer's business, not the declaration's.
 *
 * @param json the document, read only during the call; it need not be NUL-terminated. Required.
 * @param library the caller's empty declaration; filled on WEIVEL_STATUS_OK and left empty on every other answer.
 * Required.
 * @param refusal the caller's empty refusal, or NULL for none; filled on WEIVEL_STATUS_REFUSED with the field's
 * `path`, as `services[0].functions[1].symbol`, and on WEIVEL_STATUS_MALFORMED.
 * @return WEIVEL_STATUS_OK; WEIVEL_STATUS_REFUSED for malformed JSON (path "") or a field the rules refuse;
 * WEIVEL_STATUS_MALFORMED for a NULL json or library, a library or refusal below its first shape, or one still holding
 * an answer, which is left as it was; WEIVEL_STATUS_OUT_OF_MEMORY, with nothing allocated.
 */
WEIVEL_STANDARD_API weivel_status weivel_standard_read_library(const char *json, size_t length,
                                                               weivel_standard_library *library,
                                                               weivel_standard_refusal *refusal);

/** Frees what a declaration holds and leaves it empty with its `size` kept; a no-op for NULL or an empty one. */
WEIVEL_STANDARD_API void weivel_standard_library_release(weivel_standard_library *library);

#endif
