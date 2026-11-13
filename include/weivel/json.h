#ifndef WEIVEL_STANDARD_JSON_H
#define WEIVEL_STANDARD_JSON_H

#include <stddef.h>
#include <stdint.h>

#include "weivel/standard_api.h"

typedef struct weivel_json_value weivel_json_value;

typedef enum {
    WEIVEL_JSON_NULL,
    WEIVEL_JSON_BOOL,
    WEIVEL_JSON_NUMBER,
    WEIVEL_JSON_STRING,
    WEIVEL_JSON_ARRAY,
    WEIVEL_JSON_OBJECT
} weivel_json_kind;

#define WEIVEL_JSON_ERROR_DETAIL_CAPACITY 96

/**
 * message points at a string literal, or into detail when the refusal names part of the input (a
 * duplicate key), so it stays valid for as long as the weivel_json_error itself does. out_of_memory
 * is 1 when the text could not be read because memory ran out, and 0 when the text is malformed.
 */
typedef struct {
    int line;
    int column;
    const char *message;
    char detail[WEIVEL_JSON_ERROR_DETAIL_CAPACITY];
    int out_of_memory;
} weivel_json_error;

/**
 * Parses one JSON document. Returns NULL and fills error (when not NULL) on any malformed input;
 * line and column are 1-based and count bytes. The text need not be NUL-terminated.
 *
 * Refused beyond the grammar: a duplicate key within one object, a number that overflows a double,
 * text that is not well-formed UTF-8, a leading byte order mark, and a NUL in an object key.
 * The duplicate-key check compares each key with the earlier ones, so it is quadratic in the member count of one object.
 */
WEIVEL_STANDARD_API weivel_json_value *weivel_json_parse(const char *text, size_t length, weivel_json_error *error);

WEIVEL_STANDARD_API void weivel_json_free(weivel_json_value *value);

/** The kind of value, and WEIVEL_JSON_NULL for a NULL value, so an absent member reads as null. */
WEIVEL_STANDARD_API weivel_json_kind weivel_json_kind_of(const weivel_json_value *value);

/** Returns the member named key, or NULL when value is not an object or has no such member. */
WEIVEL_STANDARD_API const weivel_json_value *weivel_json_object_get(const weivel_json_value *value, const char *key);

WEIVEL_STANDARD_API size_t weivel_json_object_size(const weivel_json_value *value);
WEIVEL_STANDARD_API const char *weivel_json_object_key_at(const weivel_json_value *value, size_t index);
WEIVEL_STANDARD_API const weivel_json_value *weivel_json_object_value_at(const weivel_json_value *value, size_t index);

WEIVEL_STANDARD_API size_t weivel_json_array_length(const weivel_json_value *value);
WEIVEL_STANDARD_API const weivel_json_value *weivel_json_array_at(const weivel_json_value *value, size_t index);

/** Returns the decoded, NUL-terminated string, or NULL when value is not a string. */
WEIVEL_STANDARD_API const char *weivel_json_as_string(const weivel_json_value *value);
WEIVEL_STANDARD_API size_t weivel_json_string_length(const weivel_json_value *value);

/** Each accessor returns 1 and sets *out when value has that type, else 0. */
WEIVEL_STANDARD_API int weivel_json_as_bool(const weivel_json_value *value, int *out);
WEIVEL_STANDARD_API int weivel_json_as_int64(const weivel_json_value *value, int64_t *out);
WEIVEL_STANDARD_API int weivel_json_as_double(const weivel_json_value *value, double *out);

/**
 * Whether two values are equal as JSON Schema counts it for `uniqueItems`: the same kind, numbers equal in value
 * whatever their spelling (`1` and `1.0`), strings equal byte for byte, arrays item by item, and objects with the
 * same keys holding equal values in any order. Two NULLs are equal; a NULL and a value are not.
 */
WEIVEL_STANDARD_API int weivel_json_equal(const weivel_json_value *left, const weivel_json_value *right);

#endif
