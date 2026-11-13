#include "check.h"
#include "patterns.h"
#include "validate_rules.h"
#include "weivel/json.h"

#include <stdlib.h>

#define LOCATION_CAPACITY 1024
#define MOST_LOCATIONS 512

typedef struct pattern_binding {
    const char *location;
    weivel_standard_pattern_matcher matcher;
} pattern_binding;

/** Which matcher implements each `pattern`; the rule table pins the pattern string at the same location. */
static const pattern_binding PATTERN_BINDINGS[] = {
    {"/properties/importName/pattern", weivel_standard_matches_import_name},
    {"/properties/entry/pattern", weivel_standard_matches_entry},
    {"/properties/services/properties/consumes/items/anyOf/1/properties/provider/pattern",
     weivel_standard_matches_repository_id},
    {"/properties/dependencies/items/properties/id/pattern", weivel_standard_matches_repository_id},
    {"/properties/dependencies/items/properties/name/pattern", weivel_standard_matches_owner_and_name},
};

static const char *const ANNOTATIONS[] = {"$schema", "$id", "title", "description", "$comment"};

typedef struct found_locations {
    char *items[MOST_LOCATIONS];
    size_t count;
} found_locations;

typedef struct walk {
    found_locations constraints;
    found_locations properties;
    int rule_seen[MOST_LOCATIONS];
} walk;

static int is_annotation(const char *keyword) {
    size_t at;
    for (at = 0; at < sizeof ANNOTATIONS / sizeof ANNOTATIONS[0]; at++) {
        if (strcmp(keyword, ANNOTATIONS[at]) == 0) {
            return 1;
        }
    }
    return 0;
}

static void keep(found_locations *found, const char *location) {
    size_t length = strlen(location);
    char *copy = malloc(length + 1);
    CHECK(found->count < MOST_LOCATIONS && copy != NULL);
    if (found->count >= MOST_LOCATIONS || copy == NULL) {
        free(copy);
        return;
    }
    memcpy(copy, location, length + 1);
    found->items[found->count++] = copy;
}

static void release(found_locations *found) {
    size_t at;
    for (at = 0; at < found->count; at++) {
        free(found->items[at]);
    }
    found->count = 0;
}

static void extend(char *buffer, const char *location, const char *step) {
    int written = snprintf(buffer, LOCATION_CAPACITY, "%s/%s", location, step);
    CHECK(written > 0 && written < LOCATION_CAPACITY);
}

static const schema_rule *rule_at(const char *location, size_t *index) {
    size_t at;
    for (at = 0; at < SCHEMA_RULE_COUNT; at++) {
        if (strcmp(SCHEMA_RULES[at].location, location) == 0) {
            *index = at;
            return &SCHEMA_RULES[at];
        }
    }
    return NULL;
}

static int value_is_expected(const weivel_json_value *value, const char *expected) {
    weivel_json_error error = {0};
    weivel_json_value *parsed;
    int equal;
    if (expected == NULL) {
        return value == NULL;
    }
    if (weivel_json_kind_of(value) == WEIVEL_JSON_STRING) {
        return strlen(expected) == weivel_json_string_length(value) &&
               strcmp(weivel_json_as_string(value), expected) == 0;
    }
    parsed = weivel_json_parse(expected, strlen(expected), &error);
    equal = parsed != NULL && weivel_json_equal(parsed, value);
    weivel_json_free(parsed);
    return equal;
}

/** A constraint the schema states must be one the rule table holds, with the same value. */
static void check_constraint(walk *state, const char *location, const weivel_json_value *value) {
    size_t index = 0;
    const schema_rule *rule = rule_at(location, &index);
    keep(&state->constraints, location);
    if (rule == NULL) {
        fprintf(stderr, "the schema states %s, which the validator's rule table does not cover\n", location);
        check_failures++;
        return;
    }
    state->rule_seen[index] = 1;
    if (!value_is_expected(value, rule->expected)) {
        fprintf(stderr, "the schema's %s differs from the value the validator implements, \"%s\"\n", location,
                rule->expected == NULL ? "(none)" : rule->expected);
        check_failures++;
    }
}

static void walk_schema(walk *state, const weivel_json_value *schema, const char *location);

static void walk_properties(walk *state, const weivel_json_value *properties, const char *location) {
    char member[LOCATION_CAPACITY];
    size_t at;
    CHECK(weivel_json_kind_of(properties) == WEIVEL_JSON_OBJECT);
    for (at = 0; at < weivel_json_object_size(properties); at++) {
        extend(member, location, weivel_json_object_key_at(properties, at));
        keep(&state->properties, member);
        walk_schema(state, weivel_json_object_value_at(properties, at), member);
    }
}

static void walk_required(walk *state, const weivel_json_value *required, const char *location) {
    char name[LOCATION_CAPACITY];
    size_t at;
    CHECK(weivel_json_kind_of(required) == WEIVEL_JSON_ARRAY);
    for (at = 0; at < weivel_json_array_length(required); at++) {
        const char *field = weivel_json_as_string(weivel_json_array_at(required, at));
        CHECK(field != NULL);
        extend(name, location, field == NULL ? "" : field);
        check_constraint(state, name, NULL);
    }
}

static void walk_any_of(walk *state, const weivel_json_value *branches, const char *location) {
    char branch[LOCATION_CAPACITY];
    char index[24];
    size_t at;
    CHECK(weivel_json_kind_of(branches) == WEIVEL_JSON_ARRAY);
    for (at = 0; at < weivel_json_array_length(branches); at++) {
        snprintf(index, sizeof index, "%zu", at);
        extend(branch, location, index);
        walk_schema(state, weivel_json_array_at(branches, at), branch);
    }
}

static int holds_subschema(const char *keyword) {
    return strcmp(keyword, "items") == 0 || strcmp(keyword, "if") == 0 || strcmp(keyword, "then") == 0 ||
           strcmp(keyword, "else") == 0 || strcmp(keyword, "additionalProperties") == 0;
}

/** Invariant 5: no object closes `additionalProperties`, so a field this schema does not name still validates. */
static void check_stays_open(const char *keyword, const weivel_json_value *value, const char *location) {
    int closed = 0;
    if (strcmp(keyword, "additionalProperties") == 0 && weivel_json_as_bool(value, &closed) && !closed) {
        fprintf(stderr, "%s closes additionalProperties\n", location);
        check_failures++;
    }
}

static void walk_keyword(walk *state, const char *keyword, const weivel_json_value *value, const char *location) {
    char next[LOCATION_CAPACITY];
    extend(next, location, keyword);
    check_stays_open(keyword, value, location);
    if (strcmp(keyword, "properties") == 0) {
        walk_properties(state, value, next);
    } else if (strcmp(keyword, "required") == 0) {
        walk_required(state, value, next);
    } else if (strcmp(keyword, "anyOf") == 0) {
        walk_any_of(state, value, next);
    } else if (holds_subschema(keyword) && weivel_json_kind_of(value) == WEIVEL_JSON_OBJECT) {
        walk_schema(state, value, next);
    } else {
        check_constraint(state, next, value);
    }
}

static void walk_schema(walk *state, const weivel_json_value *schema, const char *location) {
    size_t at;
    CHECK(weivel_json_kind_of(schema) == WEIVEL_JSON_OBJECT);
    for (at = 0; at < weivel_json_object_size(schema); at++) {
        const char *keyword = weivel_json_object_key_at(schema, at);
        if (!is_annotation(keyword)) {
            walk_keyword(state, keyword, weivel_json_object_value_at(schema, at), location);
        }
    }
}

static int is_schema_rule(const schema_rule *rule) {
    return rule->location[0] == '/';
}

static void every_schema_rule_is_in_the_schema(const walk *state) {
    size_t at;
    size_t schema_rules = 0;
    for (at = 0; at < SCHEMA_RULE_COUNT; at++) {
        if (is_schema_rule(&SCHEMA_RULES[at])) {
            schema_rules++;
            if (!state->rule_seen[at]) {
                fprintf(stderr, "the rule table covers %s, which the schema does not state\n",
                        SCHEMA_RULES[at].location);
                check_failures++;
            }
        }
    }
    CHECK(schema_rules > 0 && state->constraints.count == schema_rules);
}

static void every_rule_has_a_case(void) {
    size_t rule;
    size_t test;
    for (rule = 0; rule < SCHEMA_RULE_COUNT; rule++) {
        int tested = 0;
        for (test = 0; test < RULE_CASE_COUNT && !tested; test++) {
            tested = strcmp(RULE_CASES[test].location, SCHEMA_RULES[rule].location) == 0;
        }
        if (!tested) {
            fprintf(stderr, "no case tests %s\n", SCHEMA_RULES[rule].location);
            check_failures++;
        }
    }
}

static void every_case_tests_a_rule(void) {
    size_t test;
    size_t index;
    for (test = 0; test < RULE_CASE_COUNT; test++) {
        if (rule_at(RULE_CASES[test].location, &index) == NULL) {
            fprintf(stderr, "a case tests %s, which is no rule\n", RULE_CASES[test].location);
            check_failures++;
        }
    }
}

static int starts_with(const char *text, const char *prefix) {
    size_t length = strlen(prefix);
    return strncmp(text, prefix, length) == 0 && text[length] == '/';
}

static void every_property_has_a_rule(const walk *state) {
    size_t property;
    size_t rule;
    CHECK(state->properties.count > 0);
    for (property = 0; property < state->properties.count; property++) {
        int covered = 0;
        for (rule = 0; rule < SCHEMA_RULE_COUNT && !covered; rule++) {
            covered = starts_with(SCHEMA_RULES[rule].location, state->properties.items[property]);
        }
        if (!covered) {
            fprintf(stderr, "the schema names %s, which no rule of the validator covers\n",
                    state->properties.items[property]);
            check_failures++;
        }
    }
}

static int ends_with(const char *text, const char *suffix) {
    size_t length = strlen(text);
    size_t suffix_length = strlen(suffix);
    return length >= suffix_length && strcmp(text + length - suffix_length, suffix) == 0;
}

static const pattern_binding *binding_at(const char *location) {
    size_t at;
    for (at = 0; at < sizeof PATTERN_BINDINGS / sizeof PATTERN_BINDINGS[0]; at++) {
        if (strcmp(PATTERN_BINDINGS[at].location, location) == 0) {
            return &PATTERN_BINDINGS[at];
        }
    }
    return NULL;
}

static void every_pattern_has_its_matcher(const walk *state) {
    size_t at;
    size_t index;
    size_t patterns = 0;
    for (at = 0; at < state->constraints.count; at++) {
        if (ends_with(state->constraints.items[at], "/pattern")) {
            const pattern_binding *binding = binding_at(state->constraints.items[at]);
            patterns++;
            if (binding == NULL || binding->matcher == NULL) {
                fprintf(stderr, "no hand-written matcher implements %s\n", state->constraints.items[at]);
                check_failures++;
            }
        }
    }
    for (at = 0; at < sizeof PATTERN_BINDINGS / sizeof PATTERN_BINDINGS[0]; at++) {
        CHECK(rule_at(PATTERN_BINDINGS[at].location, &index) != NULL);
    }
    CHECK(patterns == sizeof PATTERN_BINDINGS / sizeof PATTERN_BINDINGS[0]);
}

static void the_schema_names_itself(const weivel_json_value *schema) {
    check_text(weivel_json_as_string(weivel_json_object_get(schema, "$schema")),
               "http://json-schema.org/draft-07/schema#", "$schema");
    check_text(weivel_json_as_string(weivel_json_object_get(schema, "$id")),
               "https://weivel.com/schema/plugin.schema.json", "$id");
}

static char *read_file(const char *path, size_t *length) {
    FILE *file = fopen(path, "rb");
    char *text = NULL;
    long size;
    if (file == NULL) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) == 0 && (size = ftell(file)) > 0 && fseek(file, 0, SEEK_SET) == 0) {
        text = malloc((size_t)size);
        if (text != NULL && fread(text, 1, (size_t)size, file) != (size_t)size) {
            free(text);
            text = NULL;
        }
        *length = (size_t)size;
    }
    fclose(file);
    return text;
}

int main(void) {
    size_t length = 0;
    char *text = read_file(WEIVEL_STANDARD_SCHEMA_PATH, &length);
    weivel_json_error error = {0};
    weivel_json_value *schema = text == NULL ? NULL : weivel_json_parse(text, length, &error);
    static walk state;
    if (schema == NULL) {
        fprintf(stderr, "cannot read %s: %s\n", WEIVEL_STANDARD_SCHEMA_PATH,
                error.message == NULL ? "no such file" : error.message);
        free(text);
        return 1;
    }
    the_schema_names_itself(schema);
    walk_schema(&state, schema, "");
    every_schema_rule_is_in_the_schema(&state);
    every_rule_has_a_case();
    every_case_tests_a_rule();
    every_property_has_a_rule(&state);
    every_pattern_has_its_matcher(&state);
    release(&state.constraints);
    release(&state.properties);
    weivel_json_free(schema);
    free(text);
    return check_summary("schema_parity_test");
}
