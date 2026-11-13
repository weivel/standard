#include "check.h"
#include "validate_rules.h"
#include "weivel/json.h"
#include "weivel/standard.h"

#include <stdlib.h>

#define MOST_WELL_KNOWN_IDS 8

/** A test's JSON written with single quotes, turned into the double-quoted text the validator reads. */
static char *double_quoted(const char *text) {
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    size_t at;
    if (copy == NULL) {
        return NULL;
    }
    for (at = 0; at <= length; at++) {
        copy[at] = text[at] == '\'' ? '"' : text[at];
    }
    return copy;
}

static weivel_json_value *parse_quoted(const char *text) {
    char *json = double_quoted(text);
    weivel_json_error error = {0};
    weivel_json_value *value = json == NULL ? NULL : weivel_json_parse(json, strlen(json), &error);
    free(json);
    return value;
}

typedef struct host_list {
    const char *items[MOST_WELL_KNOWN_IDS];
    weivel_strings strings;
    weivel_json_value *parsed;
} host_list;

/** The options a test's `well_known` describes: NULL for none, else a list read from its JSON. */
static const weivel_standard_validation_options *options_for(const char *well_known, host_list *list,
                                                              weivel_standard_validation_options *options) {
    size_t at;
    list->parsed = NULL;
    if (well_known == NULL) {
        return NULL;
    }
    list->parsed = parse_quoted(well_known);
    CHECK(list->parsed != NULL && weivel_json_array_length(list->parsed) <= MOST_WELL_KNOWN_IDS);
    for (at = 0; at < weivel_json_array_length(list->parsed) && at < MOST_WELL_KNOWN_IDS; at++) {
        list->items[at] = weivel_json_as_string(weivel_json_array_at(list->parsed, at));
    }
    list->strings.items = list->items;
    list->strings.count = (uint32_t)at;
    options->size = (uint32_t)sizeof *options;
    options->well_known_services = &list->strings;
    return options;
}

/**
 * Validates from a heap copy that is scribbled over and freed before the answer is looked at, so every test also
 * proves the answer keeps nothing of the caller's text.
 */
static weivel_status validate_copy(const char *text, const weivel_standard_validation_options *options,
                                   weivel_standard_validation *validation) {
    char *copy = double_quoted(text);
    size_t length;
    weivel_status status;
    if (copy == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    length = strlen(copy);
    status = weivel_standard_validate(copy, length, options, validation);
    memset(copy, 'x', length);
    free(copy);
    return status;
}

static void print_paths(const weivel_standard_validation *validation) {
    uint32_t at;
    fprintf(stderr, "  got [");
    for (at = 0; at < validation->paths.count; at++) {
        fprintf(stderr, "%s\"%s\"", at == 0 ? "" : ", ", validation->paths.items[at]);
    }
    fprintf(stderr, "]\n");
}

static int paths_match(const weivel_standard_validation *validation, const weivel_json_value *expected) {
    size_t at;
    if (expected == NULL || weivel_json_array_length(expected) != validation->paths.count) {
        return 0;
    }
    for (at = 0; at < validation->paths.count; at++) {
        const char *path = weivel_json_as_string(weivel_json_array_at(expected, at));
        if (path == NULL || strcmp(path, validation->paths.items[at]) != 0) {
            return 0;
        }
    }
    return 1;
}

static void check_refusal_matches(const weivel_standard_validation *validation, uint32_t code, const char *label) {
    if (validation->paths.count == 0) {
        CHECK(validation->refusal.code == WEIVEL_STANDARD_REFUSAL_NONE && validation->refusal.storage == NULL);
        return;
    }
    if (validation->refusal.code != code) {
        fprintf(stderr, "%s: first refusal code %u, expected %u (%s)\n", label, (unsigned)validation->refusal.code,
                (unsigned)code, validation->refusal.detail == NULL ? "" : validation->refusal.detail);
        check_failures++;
    }
    check_text(validation->refusal.path, validation->paths.items[0], label);
    CHECK(validation->refusal.detail != NULL && validation->refusal.detail[0] != '\0');
    CHECK(validation->refusal.fix != NULL && validation->refusal.fix[0] != '\0');
}

static void check_validates_as(const char *label, const char *manifest, const char *well_known, const char *paths,
                               uint32_t first_code) {
    weivel_standard_validation validation = WEIVEL_INIT(weivel_standard_validation);
    weivel_standard_validation_options options = WEIVEL_INIT(weivel_standard_validation_options);
    host_list list;
    weivel_json_value *expected = parse_quoted(paths);
    weivel_status status = validate_copy(manifest, options_for(well_known, &list, &options), &validation);
    int expected_valid = expected != NULL && weivel_json_array_length(expected) == 0;
    CHECK(status == (expected_valid ? WEIVEL_STATUS_OK : WEIVEL_STATUS_REFUSED));
    CHECK(validation.valid == (uint32_t)expected_valid);
    if (!paths_match(&validation, expected)) {
        fprintf(stderr, "%s: %s expected paths %s\n", label, manifest, paths);
        print_paths(&validation);
        check_failures++;
    }
    check_refusal_matches(&validation, first_code, label);
    weivel_standard_validation_release(&validation);
    weivel_json_free(expected);
    weivel_json_free(list.parsed);
}

static void every_rule_case_validates_as_it_states(void) {
    size_t at;
    for (at = 0; at < RULE_CASE_COUNT; at++) {
        const rule_case *test = &RULE_CASES[at];
        check_validates_as(test->location, test->manifest, test->well_known, test->paths, test->first_code);
    }
}

typedef struct vector_case {
    const char *name;
    const char *manifest;
    const char *well_known;
    const char *paths;
    uint32_t first_code;
} vector_case;

/**
 * The kit's 31 `vectors/standard/validate` vectors, each manifest and well-known list as the vector gives it and the
 * paths it expects; `reports-a-missing-entry-beside-another-error` asserts `contains`, and is held here to the exact
 * list this validator answers. The four that still assert wording are held to the refusal code that wording meant.
 */
static const vector_case VECTOR_CASES[] = {
    {"accepts-a-correct-manifest",
     "{'importName': 'config-ledger', 'api': 1, 'entry': 'dist/index.js', 'capabilities': {'provides': ['provider']}}",
     NULL, "[]", WEIVEL_STANDARD_REFUSAL_NONE},
    {"accepts-a-dependency-named-by-id",
     "{'importName': 'a', 'api': 2, 'dependencies': [{'importName': 'runtime', 'id': 'forge:12/runtime'}]}", NULL,
     "[]", WEIVEL_STANDARD_REFUSAL_NONE},
    {"accepts-a-dependency-named-by-name",
     "{'importName': 'a', 'api': 2, 'dependencies': [{'importName': 'runtime', 'name': 'vendor/runtime', 'commit': "
     "'abc', 'sha256': 'def'}]}",
     NULL, "[]", WEIVEL_STANDARD_REFUSAL_NONE},
    {"accepts-a-well-known-bare-service-id", "{'importName': 'a', 'api': 2, 'services': {'provides': ['accounts']}}",
     "['accounts']", "[]", WEIVEL_STANDARD_REFUSAL_NONE},
    {"accepts-an-id-that-only-starts-with-a-reserved-name", "{'importName': 'console.foo', 'api': 2}", "[]", "[]",
     WEIVEL_STANDARD_REFUSAL_NONE},
    {"accepts-an-import-name-that-only-starts-with-package", "{'importName': 'package-lock', 'api': 2}", "[]", "[]",
     WEIVEL_STANDARD_REFUSAL_NONE},
    {"ignores-an-unknown-field",
     "{'importName': 'config-ledger', 'api': 1, 'entry': 'dist/index.js', 'capabilities': {'provides': ['provider']}, "
     "'reticulation': {'splines': 3}}",
     NULL, "[]", WEIVEL_STANDARD_REFUSAL_NONE},
    {"refuses-a-bare-service-id-that-is-not-well-known",
     "{'importName': 'a', 'api': 2, 'services': {'provides': ['routing']}}", "['accounts']",
     "['services.provides[0]']", WEIVEL_STANDARD_REFUSAL_NOT_WELL_KNOWN},
    {"refuses-a-dependency-name-that-is-a-single-dot",
     "{'importName': 'a', 'api': 2, 'dependencies': [{'importName': 'runtime', 'name': 'vendor/.'}]}", NULL,
     "['dependencies[0].name']", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE},
    {"refuses-a-dependency-name-that-is-dot-dot",
     "{'importName': 'a', 'api': 2, 'dependencies': [{'importName': 'runtime', 'name': 'vendor/..'}]}", NULL,
     "['dependencies[0].name']", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE},
    {"refuses-a-dependency-name-that-looks-like-an-id",
     "{'importName': 'a', 'api': 2, 'dependencies': [{'importName': 'runtime', 'name': 'forge:1/x'}]}", NULL,
     "['dependencies[0].name']", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE},
    {"refuses-a-dependency-with-an-id-and-a-name",
     "{'importName': 'a', 'api': 2, 'dependencies': [{'importName': 'runtime', 'id': 'forge:12/runtime', 'name': "
     "'vendor/runtime'}]}",
     NULL, "['dependencies[0].name']", WEIVEL_STANDARD_REFUSAL_CONFLICTING_FIELD},
    {"refuses-a-dependency-with-neither-an-id-nor-a-name",
     "{'importName': 'a', 'api': 2, 'dependencies': [{'importName': 'runtime'}]}", NULL, "['dependencies[0].name']",
     WEIVEL_STANDARD_REFUSAL_MISSING_FIELD},
    {"refuses-a-duplicate-capability",
     "{'importName': 'config-ledger', 'api': 2, 'entry': 'dist/index.js', 'capabilities': {'provides': ['provider', "
     "'provider']}}",
     NULL, "['capabilities.provides[1]']", WEIVEL_STANDARD_REFUSAL_DUPLICATE},
    {"refuses-a-duplicate-consumed-capability",
     "{'importName': 'config-ledger', 'api': 2, 'entry': 'dist/index.js', 'capabilities': {'consumes': ['provider', "
     "'provider']}}",
     NULL, "['capabilities.consumes[1]']", WEIVEL_STANDARD_REFUSAL_DUPLICATE},
    {"refuses-a-duplicate-permission", "{'importName': 'config-ledger', 'api': 2, 'permissions': ['net', 'net']}",
     NULL, "['permissions[1]']", WEIVEL_STANDARD_REFUSAL_DUPLICATE},
    {"refuses-a-provider-with-a-trailing-newline",
     "{'importName': 'a', 'api': 2, 'services': {'consumes': [{'id': 'x:y', 'provider': 'forge:1\\n'}]}}", "[]",
     "['services.consumes[0].provider']", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE},
    {"refuses-a-windows-reserved-id-con-before-a-dot", "{'importName': 'con.foo', 'api': 2}", "[]", "['importName']",
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE},
    {"refuses-a-windows-reserved-id-nul-before-a-dot", "{'importName': 'nul.x', 'api': 2}", "[]", "['importName']",
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE},
    {"refuses-a-windows-reserved-id", "{'importName': 'nul', 'api': 2}", "[]", "['importName']",
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE},
    {"refuses-an-entry-outside-the-repo", "{'importName': 'config-ledger', 'api': 2, 'entry': '../x.js'}", NULL,
     "['entry']", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE},
    {"refuses-an-object-entry-in-consumed-capabilities",
     "{'importName': 'config-ledger', 'api': 2, 'entry': 'dist/index.js', 'capabilities': {'consumes': [{'id': "
     "'provider'}]}}",
     NULL, "['capabilities.consumes[0]']", WEIVEL_STANDARD_REFUSAL_WRONG_TYPE},
    {"refuses-one-service-listed-in-two-forms",
     "{'importName': 'a', 'api': 2, 'services': {'provides': ['a:x', {'id': 'a:x'}]}}", "[]",
     "['services.provides[1]']", WEIVEL_STANDARD_REFUSAL_DUPLICATE},
    {"refuses-the-flat-capabilities-list",
     "{'importName': 'config-ledger', 'api': 2, 'entry': 'dist/index.js', 'capabilities': ['provider']}", NULL,
     "['capabilities']", WEIVEL_STANDARD_REFUSAL_WRONG_TYPE},
    {"refuses-the-import-name-package", "{'importName': 'package', 'api': 2}", "[]", "['importName']",
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE},
    {"reports-a-missing-entry-beside-another-error",
     "{'importName': 'config-ledger', 'api': 'one', 'capabilities': {'provides': ['provider']}}", NULL,
     "['api', 'entry']", WEIVEL_STANDARD_REFUSAL_WRONG_TYPE},
    {"reports-each-repeat-of-two-repeated-capabilities",
     "{'importName': 'config-ledger', 'api': 2, 'entry': 'dist/index.js', 'capabilities': {'provides': ['provider', "
     "'consumer', 'provider', 'consumer']}}",
     NULL, "['capabilities.provides[2]', 'capabilities.provides[3]']", WEIVEL_STANDARD_REFUSAL_DUPLICATE},
    {"reports-every-later-occurrence-of-a-repeated-capability",
     "{'importName': 'config-ledger', 'api': 2, 'entry': 'dist/index.js', 'capabilities': {'provides': ['provider', "
     "'provider', 'provider']}}",
     NULL, "['capabilities.provides[1]', 'capabilities.provides[2]']", WEIVEL_STANDARD_REFUSAL_DUPLICATE},
    {"reports-which-field-is-wrong",
     "{'importName': 'config-ledger', 'api': 'one', 'entry': 'dist/index.js', 'capabilities': {'provides': "
     "['provider']}}",
     NULL, "['api']", WEIVEL_STANDARD_REFUSAL_WRONG_TYPE},
    {"requires-an-entry-once-capabilities-are-declared",
     "{'importName': 'config-ledger', 'api': 2, 'capabilities': {'provides': ['provider']}}", NULL, "['entry']",
     WEIVEL_STANDARD_REFUSAL_MISSING_FIELD},
    {"skips-the-well-known-rule-when-the-host-names-no-list",
     "{'importName': 'a', 'api': 2, 'services': {'provides': ['routing']}}", NULL, "[]", WEIVEL_STANDARD_REFUSAL_NONE},
};

static const size_t VECTOR_CASE_COUNT = sizeof VECTOR_CASES / sizeof VECTOR_CASES[0];

static void every_kit_vector_validates_as_it_expects(void) {
    size_t at;
    CHECK(VECTOR_CASE_COUNT == 31);
    for (at = 0; at < VECTOR_CASE_COUNT; at++) {
        const vector_case *vector = &VECTOR_CASES[at];
        check_validates_as(vector->name, vector->manifest, vector->well_known, vector->paths, vector->first_code);
    }
}

typedef struct first_refusal {
    weivel_standard_validation validation;
    weivel_status status;
} first_refusal;

static first_refusal validate_with_list(const char *manifest, const char *well_known) {
    first_refusal answer = {WEIVEL_INIT(weivel_standard_validation), WEIVEL_STATUS_OK};
    weivel_standard_validation_options options = WEIVEL_INIT(weivel_standard_validation_options);
    host_list list;
    answer.status = validate_copy(manifest, options_for(well_known, &list, &options), &answer.validation);
    weivel_json_free(list.parsed);
    return answer;
}

static void check_mentions(const char *text, const char *word) {
    if (text == NULL || strstr(text, word) == NULL) {
        fprintf(stderr, "expected \"%s\" to mention \"%s\"\n", text == NULL ? "(null)" : text, word);
        check_failures++;
    }
}

/** The decision the four wording vectors carried: what their refusal says names the field and how to fix it. */
static void the_wording_vectors_say_what_they_meant(void) {
    first_refusal flat = validate_with_list(
        "{'importName': 'config-ledger', 'api': 2, 'entry': 'dist/index.js', 'capabilities': ['provider']}", NULL);
    first_refusal api = validate_with_list("{'importName': 'config-ledger', 'api': 'one'}", NULL);
    first_refusal entry =
        validate_with_list("{'importName': 'config-ledger', 'api': 2, 'capabilities': {'provides': ['provider']}}",
                           NULL);
    first_refusal bare = validate_with_list("{'importName': 'a', 'api': 2, 'services': {'provides': ['routing']}}",
                                            "['accounts', 'clock']");
    check_mentions(flat.validation.refusal.detail, "capabilities");
    check_mentions(flat.validation.refusal.fix, "provides");
    check_mentions(api.validation.refusal.detail, "api");
    check_mentions(api.validation.refusal.fix, "api");
    check_mentions(entry.validation.refusal.detail, "entry");
    check_mentions(entry.validation.refusal.fix, "entry");
    check_text(bare.validation.refusal.fix, "register it as \"a:routing\", or as one of the well-known ids: accounts, "
                                            "clock", "V2 fix");
    check_mentions(bare.validation.refusal.detail, "routing");
    weivel_standard_validation_release(&flat.validation);
    weivel_standard_validation_release(&api.validation);
    weivel_standard_validation_release(&entry.validation);
    weivel_standard_validation_release(&bare.validation);
}

static void an_empty_well_known_list_names_none(void) {
    first_refusal bare =
        validate_with_list("{'importName': 'a', 'api': 2, 'services': {'provides': ['routing']}}", "[]");
    check_text(bare.validation.refusal.fix, "register it as \"a:routing\", or as one of the well-known ids: (none)",
               "V2 fix with an empty list");
    weivel_standard_validation_release(&bare.validation);
}

static void a_refusal_says_what_and_how_to_fix_it(void) {
    first_refusal duplicate = validate_with_list("{'importName': 'a', 'api': 2, 'permissions': ['net', 'net']}", NULL);
    first_refusal missing = validate_with_list("{'api': 2}", NULL);
    first_refusal wrong = validate_with_list("{'importName': 'a', 'api': 2, 'targets': [1]}", NULL);
    check_text(duplicate.validation.refusal.detail, "permissions[1] repeats permissions[0]", "duplicate detail");
    check_text(duplicate.validation.refusal.fix, "remove the duplicate entry", "duplicate fix");
    check_text(missing.validation.refusal.detail, "importName is missing", "missing detail");
    check_text(wrong.validation.refusal.detail, "targets[0] is a number, where a string is needed", "wrong detail");
    check_text(wrong.validation.refusal.fix, "write targets[0] as a string", "wrong fix");
    weivel_standard_validation_release(&duplicate.validation);
    weivel_standard_validation_release(&missing.validation);
    weivel_standard_validation_release(&wrong.validation);
}

static void malformed_json_is_refused_with_its_place(void) {
    first_refusal answer = validate_with_list("{'importName': ", NULL);
    CHECK(answer.status == WEIVEL_STATUS_REFUSED);
    CHECK(answer.validation.refusal.code == WEIVEL_STANDARD_REFUSAL_MALFORMED_JSON);
    check_mentions(answer.validation.refusal.detail, "line 1, column");
    weivel_standard_validation_release(&answer.validation);
}

static void the_length_bounds_the_text(void) {
    const char *text = "{\"importName\": \"a\", \"api\": 2}garbage";
    weivel_standard_validation validation = WEIVEL_INIT(weivel_standard_validation);
    CHECK(weivel_standard_validate(text, strlen(text) - strlen("garbage"), NULL, &validation) == WEIVEL_STATUS_OK);
    CHECK(validation.valid == 1 && validation.paths.count == 0 && validation.storage == NULL);
    weivel_standard_validation_release(&validation);
}

static void a_release_leaves_the_validation_reusable(void) {
    first_refusal answer = validate_with_list("{}", NULL);
    CHECK(answer.validation.paths.count == 2);
    weivel_standard_validation_release(&answer.validation);
    CHECK(answer.validation.size == sizeof(weivel_standard_validation));
    CHECK(answer.validation.valid == 0 && answer.validation.paths.count == 0 && answer.validation.paths.items == NULL);
    CHECK(answer.validation.storage == NULL && answer.validation.refusal.storage == NULL);
    CHECK(weivel_standard_validate("{}", 2, NULL, &answer.validation) == WEIVEL_STATUS_REFUSED);
    weivel_standard_validation_release(&answer.validation);
    weivel_standard_validation_release(NULL);
}

static void a_malformed_call_is_answered_not_crashed(void) {
    weivel_standard_validation validation = WEIVEL_INIT(weivel_standard_validation);
    weivel_standard_validation small = {0};
    weivel_standard_validation_options options = {0};
    const char *null_item[] = {NULL};
    weivel_strings broken = {null_item, 1};
    weivel_standard_validation_options broken_options = WEIVEL_INIT(weivel_standard_validation_options);
    CHECK(weivel_standard_validate("{}", 2, NULL, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(weivel_standard_validate("{}", 2, NULL, &small) == WEIVEL_STATUS_MALFORMED);
    CHECK(small.refusal.storage == NULL);
    CHECK(weivel_standard_validate(NULL, 0, NULL, &validation) == WEIVEL_STATUS_MALFORMED);
    CHECK(validation.refusal.code == WEIVEL_STANDARD_REFUSAL_MALFORMED_CALL);
    CHECK(weivel_standard_validate("{}", 2, NULL, &validation) == WEIVEL_STATUS_MALFORMED);
    CHECK(validation.refusal.code == WEIVEL_STANDARD_REFUSAL_MALFORMED_CALL);
    weivel_standard_validation_release(&validation);
    CHECK(weivel_standard_validate("{}", 2, &options, &validation) == WEIVEL_STATUS_MALFORMED);
    CHECK(validation.refusal.code == WEIVEL_STANDARD_REFUSAL_MALFORMED_CALL);
    weivel_standard_validation_release(&validation);
    broken_options.well_known_services = &broken;
    CHECK(weivel_standard_validate("{}", 2, &broken_options, &validation) == WEIVEL_STATUS_MALFORMED);
    CHECK(validation.refusal.code == WEIVEL_STANDARD_REFUSAL_MALFORMED_CALL);
    weivel_standard_validation_release(&validation);
}

static weivel_status load_copy(const char *text, weivel_standard_manifest **manifest,
                               weivel_standard_validation *validation) {
    char *copy = double_quoted(text);
    size_t length;
    weivel_status status;
    if (copy == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    length = strlen(copy);
    status = weivel_standard_load_manifest(copy, length, NULL, manifest, validation);
    memset(copy, 'x', length);
    free(copy);
    return status;
}

static void loading_validates_before_it_reads(void) {
    weivel_standard_validation validation = WEIVEL_INIT(weivel_standard_validation);
    weivel_standard_manifest *manifest = NULL;
    CHECK(load_copy("{'importName': 'store', 'api': 2, 'services': {'provides': [{'attributes': {}}]}}", &manifest,
                    &validation) == WEIVEL_STATUS_REFUSED);
    CHECK(manifest == NULL);
    CHECK(validation.paths.count == 1 && validation.refusal.code == WEIVEL_STANDARD_REFUSAL_MISSING_FIELD);
    check_text(validation.refusal.path, "services.provides[0].id", "load refusal path");
    weivel_standard_validation_release(&validation);

    CHECK(load_copy("{'importName': 'store', 'api': 2, 'entry': 'lib/store.so'}", &manifest, &validation) ==
          WEIVEL_STATUS_OK);
    CHECK(manifest != NULL && validation.valid == 1 && validation.paths.count == 0);
    if (manifest != NULL) {
        check_text(weivel_standard_manifest_view(manifest)->import_name, "store", "loaded importName");
        CHECK(weivel_standard_manifest_view(manifest)->api == 2);
    }
    weivel_standard_manifest_release(manifest);
    weivel_standard_validation_release(&validation);
}

static void loading_refuses_what_the_core_cannot_hold(void) {
    weivel_standard_validation validation = WEIVEL_INIT(weivel_standard_validation);
    weivel_standard_manifest *manifest = NULL;
    CHECK(load_copy("{'importName': 'store', 'api': 5000000000}", &manifest, &validation) == WEIVEL_STATUS_REFUSED);
    CHECK(manifest == NULL && validation.valid == 0);
    CHECK(validation.refusal.code == WEIVEL_STANDARD_REFUSAL_UNREPRESENTABLE);
    CHECK(validation.paths.count == 1);
    if (validation.paths.count == 1) {
        check_text(validation.paths.items[0], "api", "unrepresentable path");
    }
    weivel_standard_validation_release(&validation);
    CHECK(validation.paths.count == 0 && validation.storage == NULL);
}

static void reading_alone_does_not_validate(void) {
    weivel_standard_manifest *manifest = NULL;
    weivel_standard_refusal refusal = {0};
    const char *text = "{\"api\": 0, \"importName\": \"package\"}";
    CHECK(weivel_standard_read_manifest(text, strlen(text), &manifest, &refusal) == WEIVEL_STATUS_OK);
    weivel_standard_manifest_release(manifest);
    weivel_standard_refusal_release(&refusal);
}

static void a_malformed_load_is_answered_not_crashed(void) {
    weivel_standard_validation validation = WEIVEL_INIT(weivel_standard_validation);
    weivel_standard_manifest *manifest = (weivel_standard_manifest *)&validation;
    CHECK(weivel_standard_load_manifest("{}", 2, NULL, NULL, &validation) == WEIVEL_STATUS_MALFORMED);
    CHECK(validation.refusal.code == WEIVEL_STANDARD_REFUSAL_MALFORMED_CALL);
    weivel_standard_validation_release(&validation);
    CHECK(weivel_standard_load_manifest("{}", 2, NULL, NULL, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(weivel_standard_load_manifest(NULL, 0, NULL, &manifest, &validation) == WEIVEL_STATUS_MALFORMED);
    CHECK(manifest == NULL);
    weivel_standard_validation_release(&validation);
}

int main(void) {
    every_rule_case_validates_as_it_states();
    every_kit_vector_validates_as_it_expects();
    the_wording_vectors_say_what_they_meant();
    an_empty_well_known_list_names_none();
    a_refusal_says_what_and_how_to_fix_it();
    malformed_json_is_refused_with_its_place();
    the_length_bounds_the_text();
    a_release_leaves_the_validation_reusable();
    a_malformed_call_is_answered_not_crashed();
    loading_validates_before_it_reads();
    loading_refuses_what_the_core_cannot_hold();
    reading_alone_does_not_validate();
    a_malformed_load_is_answered_not_crashed();
    return check_summary("validate_test");
}
