#ifndef WEIVEL_STANDARD_JSON_EQUALITY_H
#define WEIVEL_STANDARD_JSON_EQUALITY_H

#include "weivel/json.h"

/**
 * Whether two values are equal as JSON Schema counts it for `uniqueItems`: the same kind, numbers equal in value
 * whatever their spelling (`1` and `1.0`), strings equal byte for byte, arrays item by item, and objects with the
 * same keys holding equal values in any order. Two NULLs are equal; a NULL and a value are not. Two numbers beyond
 * 2^53 are equal only when both were read as the same 64-bit integer.
 */
int weivel_standard_json_equal(const weivel_json_value *left, const weivel_json_value *right);

#endif
