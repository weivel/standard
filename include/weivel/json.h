#ifndef WEIVEL_STANDARD_JSON_H
#define WEIVEL_STANDARD_JSON_H

#include <stddef.h>
#include <stdint.h>

typedef struct json_value json_value;

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_kind;

#define JSON_ERROR_DETAIL_CAPACITY 96

/**
 * message points at a string literal, or into detail when the refusal names part of the input (a
 * duplicate key), so it stays valid for as long as the json_error itself does.
 */
typedef struct {
    int line;
    int column;
    const char *message;
    char detail[JSON_ERROR_DETAIL_CAPACITY];
} json_error;

/**
 * Parses one JSON document. Returns NULL and fills error (when not NULL) on any malformed input;
 * line and column are 1-based and count bytes. The text need not be NUL-terminated.
 *
 * Refused beyond the grammar: a duplicate key within one object, a number that overflows a double,
 * text that is not well-formed UTF-8, and a leading byte order mark.
 */
json_value *json_parse(const char *text, size_t length, json_error *error);

void json_free(json_value *value);

/** The kind of value, and JSON_NULL for a NULL value, so an absent member reads as null. */
json_kind json_kind_of(const json_value *value);

/** Returns the member named key, or NULL when value is not an object or has no such member. */
const json_value *json_object_get(const json_value *value, const char *key);

size_t json_object_size(const json_value *value);
const char *json_object_key_at(const json_value *value, size_t index);
const json_value *json_object_value_at(const json_value *value, size_t index);

size_t json_array_length(const json_value *value);
const json_value *json_array_at(const json_value *value, size_t index);

/** Returns the decoded, NUL-terminated string, or NULL when value is not a string. */
const char *json_as_string(const json_value *value);
size_t json_string_length(const json_value *value);

/** Each accessor returns 1 and sets *out when value has that type, else 0. */
int json_as_bool(const json_value *value, int *out);
int json_as_int64(const json_value *value, int64_t *out);
int json_as_double(const json_value *value, double *out);

#endif
