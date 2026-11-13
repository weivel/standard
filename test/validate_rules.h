#ifndef WEIVEL_STANDARD_TEST_VALIDATE_RULES_H
#define WEIVEL_STANDARD_TEST_VALIDATE_RULES_H

#include <stddef.h>
#include <stdint.h>

/**
 * One rule the validator implements. A schema rule's `location` is a JSON pointer to one constraint keyword of
 * `schema/plugin.schema.json`, with `required/<name>` for each name a `required` lists, and `expected` is that
 * keyword's value: a string's own text, or JSON text for any other value, NULL for a required name. A rule from
 * VALIDATION.md has a location that does not start with `/` and no expected value.
 */
typedef struct schema_rule {
    const char *location;
    const char *expected;
} schema_rule;

/**
 * One test of one rule. `manifest`, `well_known` and `paths` are JSON written with single quotes for double ones;
 * `well_known` is NULL when the host passes no list, and `paths` is the exact list the validation answers, in order.
 * `first_code` is the refusal code of the first path, WEIVEL_STANDARD_REFUSAL_NONE for a valid manifest.
 */
typedef struct rule_case {
    const char *location;
    const char *manifest;
    const char *well_known;
    const char *paths;
    uint32_t first_code;
} rule_case;

extern const schema_rule SCHEMA_RULES[];
extern const size_t SCHEMA_RULE_COUNT;
extern const rule_case RULE_CASES[];
extern const size_t RULE_CASE_COUNT;

#endif
