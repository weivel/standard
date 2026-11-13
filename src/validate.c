#include "weivel/standard.h"

#include "allocation.h"
#include "json_equality.h"
#include "patterns.h"
#include "report.h"
#include "weivel/json.h"

#include <string.h>

typedef weivel_standard_field_path field_path;
typedef weivel_standard_refusal_texts refusal_texts;

#define VALIDATION_FIRST_SHAPE (offsetof(weivel_standard_validation, storage) + sizeof(void *))
#define OPTIONS_FIRST_SHAPE (offsetof(weivel_standard_validation_options, well_known_services) + sizeof(void *))

typedef struct found_paths {
    char **items;
    size_t count;
    size_t capacity;
} found_paths;

typedef struct validating {
    found_paths found;
    weivel_standard_refusal refusal;
    const weivel_strings *well_known;
    const char *import_name;
    int out_of_memory;
} validating;

/** A `pattern` the schema states, the matcher that decides it, and what a refusal of it says. */
typedef struct pattern_rule {
    weivel_standard_pattern_matcher matches;
    const char *what;
    const char *fix;
} pattern_rule;

static const char *const IMPORT_NAME_FIX =
    "use lowercase words joined by single hyphens or dots, for example \"text-store\" or \"store.cache\", and not "
    "a Windows device name with or without a dot after it (con, prn, aux, nul, com1 to com9, lpt1 to "
    "lpt9)";
static const char *const API_FIX = "set api to the lowest API major version this plugin needs, for example 1";
static const char *const ENTRY_FIX =
    "point entry at the built module a host loads, inside the plugin's directory, for example \"lib/plugin.so\", with "
    "no leading slash, drive letter, control character or .. segment";
static const char *const ENTRY_REQUIRED_FIX =
    "capabilities are provided, so add entry naming the module that provides them, for example \"lib/plugin.so\"";
static const char *const CAPABILITIES_FIX =
    "write capabilities as an object, for example {\"provides\": [\"screens\"], \"consumes\": [\"provider\"]}; a flat "
    "list of ids moves under \"provides\"";
static const char *const REPOSITORY_ID_FIX =
    "write a repository id, local:<name> or <forge>[@host]:<number> then path segments, for example \"local:store\" "
    "or \"forge@git.example.com:12/runtime\"";
static const char *const SERVICE_FIX =
    "write the service as its id, for example \"store:read\", or as an object carrying \"id\"";
static const char *const DUPLICATE_FIX = "remove the duplicate entry";

static const pattern_rule IMPORT_NAME_PATTERN = {weivel_standard_matches_import_name, "is not a valid import name",
                                                 NULL};
static const pattern_rule ENTRY_PATTERN = {weivel_standard_matches_entry,
                                           "leaves the plugin's directory or holds a control character", NULL};
static const pattern_rule REPOSITORY_ID_PATTERN = {weivel_standard_matches_repository_id, "is not a repository id",
                                                   NULL};
static const pattern_rule OWNER_AND_NAME_PATTERN = {
    weivel_standard_matches_owner_and_name, "is not owner/name",
    "write the dependency name as owner/name, for example \"vendor/runtime\"; a dependency carries an id or a name, "
    "never both"};

static void out_of_memory(validating *state) {
    state->out_of_memory = 1;
}

static int is_listed(const validating *state, const char *path) {
    size_t at;
    for (at = 0; at < state->found.count; at++) {
        if (strcmp(state->found.items[at], path) == 0) {
            return 1;
        }
    }
    return 0;
}

static int keep_path(validating *state, char *path) {
    if (state->found.count == state->found.capacity) {
        size_t wanted = state->found.capacity == 0 ? 8 : state->found.capacity * 2;
        char **grown = weivel_standard_memory_reallocate(state->found.items, wanted * sizeof *grown);
        if (grown == NULL) {
            return 0;
        }
        state->found.items = grown;
        state->found.capacity = wanted;
    }
    state->found.items[state->found.count++] = path;
    return 1;
}

/**
 * Lists the path once. Answers its text only when it is the first finding, which the caller makes the refusal with
 * record, so a later finding allocates no text it would throw away; NULL otherwise, or when memory ran out.
 */
static char *claim_path(validating *state, const field_path *path) {
    char *text;
    if (state->out_of_memory) {
        return NULL;
    }
    text = weivel_standard_path_text(path);
    if (text == NULL) {
        out_of_memory(state);
        return NULL;
    }
    if (is_listed(state, text)) {
        weivel_standard_memory_release(text);
        return NULL;
    }
    if (state->found.count == 0) {
        return text;
    }
    if (!keep_path(state, text)) {
        weivel_standard_memory_release(text);
        out_of_memory(state);
    }
    return NULL;
}

/** Lists the first finding's `path` and makes the refusal of it, taking `detail` and `fix`. */
static void record(validating *state, char *path, uint32_t code, char *detail, char *fix) {
    refusal_texts texts = {NULL, NULL, NULL};
    texts.detail = detail;
    texts.fix = fix;
    if (!keep_path(state, path)) {
        weivel_standard_memory_release(path);
        weivel_standard_release_texts(&texts);
        out_of_memory(state);
        return;
    }
    texts.path = weivel_standard_format_text("%s", path);
    if (!weivel_standard_fill_refusal(&state->refusal, code, texts)) {
        out_of_memory(state);
    }
}

/** `detail_format` holds one %s, which names the field. */
static void report(validating *state, const field_path *path, uint32_t code, const char *detail_format,
                   const char *fix) {
    char *text = claim_path(state, path);
    const char *subject;
    if (text == NULL) {
        return;
    }
    subject = weivel_standard_subject_of(text);
    record(state, text, code, weivel_standard_format_text(detail_format, subject),
           weivel_standard_format_text("%s", fix));
}

static void report_wrong_type(validating *state, const weivel_json_value *value, const field_path *path,
                              const char *expected, const char *fix) {
    char *text = claim_path(state, path);
    const char *subject;
    if (text == NULL) {
        return;
    }
    subject = weivel_standard_subject_of(text);
    record(state, text, WEIVEL_STANDARD_REFUSAL_WRONG_TYPE,
           weivel_standard_format_text("%s is %s, where %s is needed", subject, weivel_standard_kind_phrase(value),
                                       expected),
           fix == NULL ? weivel_standard_format_text("write %s as %s", subject, expected)
                       : weivel_standard_format_text("%s", fix));
}

static void report_missing(validating *state, const field_path *parent, const char *key, const char *fix) {
    field_path missing = {parent, key, 0};
    report(state, &missing, WEIVEL_STANDARD_REFUSAL_MISSING_FIELD, "%s is missing", fix);
}

static int is_kind(const weivel_json_value *value, weivel_json_kind kind) {
    return value != NULL && weivel_json_kind_of(value) == kind;
}

static int check_type(validating *state, const weivel_json_value *value, const field_path *path,
                      weivel_json_kind kind, const char *expected, const char *fix) {
    if (is_kind(value, kind)) {
        return 1;
    }
    report_wrong_type(state, value, path, expected, fix);
    return 0;
}

static int check_string(validating *state, const weivel_json_value *value, const field_path *path,
                        const char *fix) {
    return check_type(state, value, path, WEIVEL_JSON_STRING, "a string", fix);
}

static void check_pattern(validating *state, const weivel_json_value *value, const field_path *path,
                          const pattern_rule *rule, const char *fix) {
    const char *text = weivel_json_as_string(value);
    char *path_text;
    if (text == NULL || rule->matches(text, weivel_json_string_length(value))) {
        return;
    }
    path_text = claim_path(state, path);
    if (path_text == NULL) {
        return;
    }
    record(state, path_text, WEIVEL_STANDARD_REFUSAL_INVALID_VALUE,
           weivel_standard_format_text("%s \"%s\" %s", weivel_standard_subject_of(path_text), text, rule->what),
           weivel_standard_format_text("%s", rule->fix != NULL ? rule->fix : fix));
}

static void check_patterned_string(validating *state, const weivel_json_value *value, const field_path *path,
                                   const pattern_rule *rule, const char *fix) {
    if (check_string(state, value, path, fix)) {
        check_pattern(state, value, path, rule, fix);
    }
}

static void check_optional_string(validating *state, const weivel_json_value *object, const field_path *parent,
                                  const char *key) {
    const weivel_json_value *value = weivel_json_object_get(object, key);
    field_path member = {parent, key, 0};
    if (value != NULL) {
        check_string(state, value, &member, NULL);
    }
}

static void report_duplicate(validating *state, const field_path *path, size_t earlier) {
    field_path first = {path->parent, NULL, earlier};
    char *text = claim_path(state, path);
    char *first_text;
    if (text == NULL) {
        return;
    }
    first_text = weivel_standard_path_text(&first);
    if (first_text == NULL) {
        weivel_standard_memory_release(text);
        out_of_memory(state);
        return;
    }
    record(state, text, WEIVEL_STANDARD_REFUSAL_DUPLICATE,
           weivel_standard_format_text("%s repeats %s", text, first_text),
           weivel_standard_format_text("%s", DUPLICATE_FIX));
    weivel_standard_memory_release(first_text);
}

/** `uniqueItems`: an item equal to an earlier one is reported at its own index. */
static void check_unique_items(validating *state, const weivel_json_value *array, const field_path *path) {
    size_t length = weivel_json_array_length(array);
    size_t at;
    size_t earlier;
    for (at = 1; at < length; at++) {
        for (earlier = 0; earlier < at; earlier++) {
            if (weivel_standard_json_equal(weivel_json_array_at(array, earlier), weivel_json_array_at(array, at))) {
                field_path item = {path, NULL, at};
                report_duplicate(state, &item, earlier);
                break;
            }
        }
    }
}

static void check_string_list(validating *state, const weivel_json_value *value, const field_path *path,
                              int unique_items, const char *fix) {
    size_t length;
    size_t at;
    if (value == NULL || !check_type(state, value, path, WEIVEL_JSON_ARRAY, "an array", fix)) {
        return;
    }
    length = weivel_json_array_length(value);
    for (at = 0; at < length; at++) {
        field_path item = {path, NULL, at};
        check_string(state, weivel_json_array_at(value, at), &item, fix);
    }
    if (unique_items) {
        check_unique_items(state, value, path);
    }
}

/** `additionalProperties: {"type": "number"}`, as `attributes` and `constraints` are written. */
static void check_named_numbers(validating *state, const weivel_json_value *value, const field_path *path) {
    size_t count;
    size_t at;
    if (value == NULL || !check_type(state, value, path, WEIVEL_JSON_OBJECT, "an object", NULL)) {
        return;
    }
    count = weivel_json_object_size(value);
    for (at = 0; at < count; at++) {
        field_path member = {path, weivel_json_object_key_at(value, at), 0};
        check_type(state, weivel_json_object_value_at(value, at), &member, WEIVEL_JSON_NUMBER, "a number", NULL);
    }
}

static void check_optional_boolean(validating *state, const weivel_json_value *object, const field_path *parent,
                                   const char *key) {
    const weivel_json_value *value = weivel_json_object_get(object, key);
    field_path member = {parent, key, 0};
    if (value != NULL) {
        check_type(state, value, &member, WEIVEL_JSON_BOOL, "a boolean", NULL);
    }
}

static void check_import_name(validating *state, const weivel_json_value *root) {
    const weivel_json_value *value = weivel_json_object_get(root, "importName");
    field_path path = {NULL, "importName", 0};
    if (value == NULL) {
        report_missing(state, NULL, "importName", IMPORT_NAME_FIX);
        return;
    }
    check_patterned_string(state, value, &path, &IMPORT_NAME_PATTERN, IMPORT_NAME_FIX);
}

static void check_api(validating *state, const weivel_json_value *root) {
    const weivel_json_value *value = weivel_json_object_get(root, "api");
    field_path path = {NULL, "api", 0};
    double level = 0;
    if (value == NULL) {
        report_missing(state, NULL, "api", API_FIX);
        return;
    }
    if (!weivel_json_as_double(value, &level) || !weivel_standard_is_whole_number(level)) {
        report_wrong_type(state, value, &path, "a whole number", API_FIX);
        return;
    }
    if (level < 1) {
        report(state, &path, WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "%s is below 1, the lowest API version", API_FIX);
    }
}

static void check_entry(validating *state, const weivel_json_value *root) {
    const weivel_json_value *value = weivel_json_object_get(root, "entry");
    field_path path = {NULL, "entry", 0};
    if (value != NULL) {
        check_patterned_string(state, value, &path, &ENTRY_PATTERN, ENTRY_FIX);
    }
}

static void check_capabilities(validating *state, const weivel_json_value *root) {
    const weivel_json_value *value = weivel_json_object_get(root, "capabilities");
    field_path path = {NULL, "capabilities", 0};
    field_path provides = {&path, "provides", 0};
    field_path consumes = {&path, "consumes", 0};
    if (value == NULL || !check_type(state, value, &path, WEIVEL_JSON_OBJECT, "an object", CAPABILITIES_FIX)) {
        return;
    }
    check_string_list(state, weivel_json_object_get(value, "provides"), &provides, 1, NULL);
    check_string_list(state, weivel_json_object_get(value, "consumes"), &consumes, 1, NULL);
}

/** The service an entry names, in either form; NULL when the entry carries no id as a string. */
static const char *service_id_of(const weivel_json_value *entry) {
    if (is_kind(entry, WEIVEL_JSON_OBJECT)) {
        return weivel_json_as_string(weivel_json_object_get(entry, "id"));
    }
    return weivel_json_as_string(entry);
}

static size_t service_id_length(const weivel_json_value *entry) {
    if (is_kind(entry, WEIVEL_JSON_OBJECT)) {
        return weivel_json_string_length(weivel_json_object_get(entry, "id"));
    }
    return weivel_json_string_length(entry);
}

static int names_same_service(const weivel_json_value *left, const weivel_json_value *right) {
    const char *left_id = service_id_of(left);
    const char *right_id = service_id_of(right);
    size_t length = service_id_length(left);
    return left_id != NULL && right_id != NULL && length == service_id_length(right) &&
           memcmp(left_id, right_id, length) == 0;
}

/** V1: a service listed again, in either form, is reported at its later index. */
static void check_listed_once(validating *state, const weivel_json_value *list, const field_path *path, size_t at) {
    size_t earlier;
    for (earlier = 0; earlier < at; earlier++) {
        if (names_same_service(weivel_json_array_at(list, earlier), weivel_json_array_at(list, at))) {
            field_path item = {path, NULL, at};
            report_duplicate(state, &item, earlier);
            return;
        }
    }
}

static int is_well_known(const weivel_strings *well_known, const char *id) {
    uint32_t at;
    for (at = 0; at < well_known->count; at++) {
        if (strcmp(well_known->items[at], id) == 0) {
            return 1;
        }
    }
    return 0;
}

/** `<importName>:<name>`, both parts non-empty, the namespace being everything before the first colon. */
static int is_namespaced(const char *import_name, const char *id) {
    const char *separator = strchr(id, ':');
    size_t length;
    if (import_name == NULL || separator == NULL || separator == id || separator[1] == '\0') {
        return 0;
    }
    length = (size_t)(separator - id);
    return strlen(import_name) == length && memcmp(import_name, id, length) == 0;
}

static size_t joined_length(const weivel_strings *ids) {
    size_t length = ids->count == 0 ? strlen("(none)") : 0;
    uint32_t at;
    for (at = 0; at < ids->count; at++) {
        length += strlen(ids->items[at]) + (at == 0 ? 0 : 2);
    }
    return length;
}

/** The host's well-known ids joined by ", ", or "(none)"; NULL when memory runs out. */
static char *join_well_known(const weivel_strings *ids) {
    char *text = weivel_standard_memory_allocate(joined_length(ids) + 1);
    char *cursor = text;
    uint32_t at;
    if (text == NULL) {
        return NULL;
    }
    if (ids->count == 0) {
        memcpy(cursor, "(none)", 6);
        cursor += 6;
    }
    for (at = 0; at < ids->count; at++) {
        size_t part = strlen(ids->items[at]);
        if (at > 0) {
            memcpy(cursor, ", ", 2);
            cursor += 2;
        }
        memcpy(cursor, ids->items[at], part);
        cursor += part;
    }
    *cursor = '\0';
    return text;
}

static const char *name_part_of(const char *id) {
    const char *name = id;
    const char *cursor;
    for (cursor = id; *cursor != '\0'; cursor++) {
        if (*cursor == ':') {
            name = cursor + 1;
        }
    }
    return name;
}

static void report_not_well_known(validating *state, const field_path *path, const char *id) {
    char *text = claim_path(state, path);
    char *known;
    if (text == NULL) {
        return;
    }
    known = join_well_known(state->well_known);
    if (known == NULL) {
        weivel_standard_memory_release(text);
        out_of_memory(state);
        return;
    }
    record(state, text, WEIVEL_STANDARD_REFUSAL_NOT_WELL_KNOWN,
           weivel_standard_format_text("%s, service \"%s\", is neither namespaced by this plugin's importName nor "
                                       "well-known to this host",
                                       text, id),
           weivel_standard_format_text("register it as \"%s:%s\", or as one of the well-known ids: %s",
                                       state->import_name == NULL ? "<importName>" : state->import_name,
                                       name_part_of(id), known));
    weivel_standard_memory_release(known);
}

static int holds_nul(const weivel_json_value *entry, const char *id) {
    return strlen(id) != service_id_length(entry);
}

/** V2, only when the host passed a well-known list; an id holding a NUL is neither well-known nor namespaced. */
static void check_namespaced(validating *state, const weivel_json_value *entry, const field_path *path) {
    const char *id = service_id_of(entry);
    if (state->well_known == NULL || id == NULL) {
        return;
    }
    if (holds_nul(entry, id) || (!is_well_known(state->well_known, id) && !is_namespaced(state->import_name, id))) {
        report_not_well_known(state, path, id);
    }
}

static void check_service_id(validating *state, const weivel_json_value *entry, const field_path *path) {
    const weivel_json_value *id = weivel_json_object_get(entry, "id");
    field_path member = {path, "id", 0};
    if (id == NULL) {
        report_missing(state, path, "id", "add \"id\", the service id");
        return;
    }
    check_string(state, id, &member, NULL);
}

static void check_offer_object(validating *state, const weivel_json_value *entry, const field_path *path) {
    field_path attributes = {path, "attributes", 0};
    check_service_id(state, entry, path);
    check_named_numbers(state, weivel_json_object_get(entry, "attributes"), &attributes);
}

static void check_want_object(validating *state, const weivel_json_value *entry, const field_path *path) {
    const weivel_json_value *provider = weivel_json_object_get(entry, "provider");
    field_path provider_path = {path, "provider", 0};
    field_path constraints = {path, "constraints", 0};
    check_service_id(state, entry, path);
    if (provider != NULL) {
        check_patterned_string(state, provider, &provider_path, &REPOSITORY_ID_PATTERN, REPOSITORY_ID_FIX);
    }
    check_optional_boolean(state, entry, path, "optional");
    check_optional_boolean(state, entry, path, "degradable");
    check_named_numbers(state, weivel_json_object_get(entry, "constraints"), &constraints);
}

typedef void (*object_entry_check)(validating *state, const weivel_json_value *entry, const field_path *path);

/**
 * `anyOf: [string, object]`, the branch chosen by the entry's JSON type, so a fault inside an object is named at
 * its own field rather than at the entry.
 */
static void check_service_entry(validating *state, const weivel_json_value *entry, const field_path *path,
                                object_entry_check check_object) {
    if (is_kind(entry, WEIVEL_JSON_OBJECT)) {
        check_object(state, entry, path);
    } else if (!is_kind(entry, WEIVEL_JSON_STRING)) {
        report_wrong_type(state, entry, path, "a string or an object", SERVICE_FIX);
    }
}

static void check_service_list(validating *state, const weivel_json_value *list, const field_path *path,
                               object_entry_check check_object, int is_provided) {
    size_t length;
    size_t at;
    if (list == NULL || !check_type(state, list, path, WEIVEL_JSON_ARRAY, "an array", NULL)) {
        return;
    }
    length = weivel_json_array_length(list);
    for (at = 0; at < length; at++) {
        const weivel_json_value *entry = weivel_json_array_at(list, at);
        field_path item = {path, NULL, at};
        check_service_entry(state, entry, &item, check_object);
        check_listed_once(state, list, path, at);
        if (is_provided) {
            check_namespaced(state, entry, &item);
        }
    }
}

static void check_services(validating *state, const weivel_json_value *root) {
    const weivel_json_value *value = weivel_json_object_get(root, "services");
    field_path path = {NULL, "services", 0};
    field_path provides = {&path, "provides", 0};
    field_path consumes = {&path, "consumes", 0};
    if (value == NULL || !check_type(state, value, &path, WEIVEL_JSON_OBJECT, "an object", NULL)) {
        return;
    }
    check_service_list(state, weivel_json_object_get(value, "provides"), &provides, check_offer_object, 1);
    check_service_list(state, weivel_json_object_get(value, "consumes"), &consumes, check_want_object, 0);
}

/** The schema's `if`: `id` present as a string. `then` refuses a `name` beside it; `else` requires one. */
static void check_dependency_locator(validating *state, const weivel_json_value *dependency, const field_path *path) {
    const weivel_json_value *name = weivel_json_object_get(dependency, "name");
    field_path name_path = {path, "name", 0};
    if (is_kind(weivel_json_object_get(dependency, "id"), WEIVEL_JSON_STRING)) {
        if (name != NULL) {
            report(state, &name_path, WEIVEL_STANDARD_REFUSAL_CONFLICTING_FIELD,
                   "%s is given beside id, and a dependency carries one of them", "remove name, or remove id");
        }
        return;
    }
    if (name == NULL) {
        report_missing(state, path, "name",
                       "name the dependency by \"id\", a repository id, or by \"name\", owner/name");
    }
}

static void check_dependency(validating *state, const weivel_json_value *dependency, const field_path *path) {
    const weivel_json_value *id = weivel_json_object_get(dependency, "id");
    const weivel_json_value *name = weivel_json_object_get(dependency, "name");
    field_path import_name = {path, "importName", 0};
    field_path id_path = {path, "id", 0};
    field_path name_path = {path, "name", 0};
    if (!check_type(state, dependency, path, WEIVEL_JSON_OBJECT, "an object", NULL)) {
        return;
    }
    if (weivel_json_object_get(dependency, "importName") == NULL) {
        report_missing(state, path, "importName",
                       "add \"importName\", the name this plugin imports the dependency by");
    } else {
        check_string(state, weivel_json_object_get(dependency, "importName"), &import_name, NULL);
    }
    if (id != NULL) {
        check_patterned_string(state, id, &id_path, &REPOSITORY_ID_PATTERN, REPOSITORY_ID_FIX);
    }
    if (name != NULL) {
        check_patterned_string(state, name, &name_path, &OWNER_AND_NAME_PATTERN, NULL);
    }
    check_optional_string(state, dependency, path, "commit");
    check_optional_string(state, dependency, path, "sha256");
    check_dependency_locator(state, dependency, path);
}

static void check_dependencies(validating *state, const weivel_json_value *root) {
    const weivel_json_value *value = weivel_json_object_get(root, "dependencies");
    field_path path = {NULL, "dependencies", 0};
    size_t length;
    size_t at;
    if (value == NULL || !check_type(state, value, &path, WEIVEL_JSON_ARRAY, "an array", NULL)) {
        return;
    }
    length = weivel_json_array_length(value);
    for (at = 0; at < length; at++) {
        field_path item = {&path, NULL, at};
        check_dependency(state, weivel_json_array_at(value, at), &item);
    }
}

/** The schema's `if`: capabilities is an object whose `provides` is an array of at least one item. */
static int provides_capabilities(const weivel_json_value *root) {
    const weivel_json_value *capabilities = weivel_json_object_get(root, "capabilities");
    const weivel_json_value *provides = weivel_json_object_get(capabilities, "provides");
    return is_kind(capabilities, WEIVEL_JSON_OBJECT) && is_kind(provides, WEIVEL_JSON_ARRAY) &&
           weivel_json_array_length(provides) >= 1;
}

/** The schema's `then`: an entry that is a string of at least one character. */
static void check_entry_once_capabilities_are_provided(validating *state, const weivel_json_value *root) {
    const weivel_json_value *entry = weivel_json_object_get(root, "entry");
    field_path path = {NULL, "entry", 0};
    if (!provides_capabilities(root)) {
        return;
    }
    if (entry == NULL) {
        report_missing(state, NULL, "entry", ENTRY_REQUIRED_FIX);
        return;
    }
    if (check_string(state, entry, &path, ENTRY_FIX) && weivel_json_string_length(entry) == 0) {
        report(state, &path, WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "%s is empty", ENTRY_REQUIRED_FIX);
    }
}

static void check_document(validating *state, const weivel_json_value *root) {
    field_path permissions = {NULL, "permissions", 0};
    field_path targets = {NULL, "targets", 0};
    if (!check_type(state, root, NULL, WEIVEL_JSON_OBJECT, "an object", NULL)) {
        return;
    }
    state->import_name = weivel_json_as_string(weivel_json_object_get(root, "importName"));
    check_import_name(state, root);
    check_api(state, root);
    check_entry(state, root);
    check_string_list(state, weivel_json_object_get(root, "targets"), &targets, 0, NULL);
    check_capabilities(state, root);
    check_services(state, root);
    check_string_list(state, weivel_json_object_get(root, "permissions"), &permissions, 1, NULL);
    check_dependencies(state, root);
    check_entry_once_capabilities_are_provided(state, root);
}

static void release_found(found_paths *found) {
    size_t at;
    for (at = 0; at < found->count; at++) {
        weivel_standard_memory_release(found->items[at]);
    }
    weivel_standard_memory_release(found->items);
}

static void release_state(validating *state) {
    release_found(&state->found);
    weivel_standard_refusal_release(&state->refusal);
}

/** Hands what was found to the caller's struct; `storage` is the path array, which owns every path. */
static void publish(validating *state, weivel_standard_validation *validation) {
    validation->valid = state->found.count == 0;
    validation->paths.items = (const char *const *)state->found.items;
    validation->paths.count = (uint32_t)state->found.count;
    validation->refusal = state->refusal;
    validation->storage = state->found.items;
}

static weivel_status finish(validating *state, weivel_standard_validation *validation) {
    if (state->out_of_memory) {
        release_state(state);
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    publish(state, validation);
    return validation->valid ? WEIVEL_STATUS_OK : WEIVEL_STATUS_REFUSED;
}

static void record_malformed_json(validating *state, const weivel_json_error *error) {
    char *path = weivel_standard_format_text("%s", "");
    if (path == NULL) {
        out_of_memory(state);
        return;
    }
    record(state, path, WEIVEL_STANDARD_REFUSAL_MALFORMED_JSON,
           weivel_standard_format_text("line %d, column %d: %s", error->line, error->column,
                                       error->message == NULL ? "malformed JSON" : error->message),
           weivel_standard_format_text("correct the JSON at line %d, column %d", error->line, error->column));
}

static int holds_answer(const weivel_standard_validation *validation) {
    return validation->storage != NULL || validation->paths.count != 0 || validation->paths.items != NULL ||
           validation->refusal.storage != NULL || validation->refusal.code != WEIVEL_STANDARD_REFUSAL_NONE;
}

static int is_usable(const weivel_standard_validation *validation) {
    return validation != NULL && validation->size >= VALIDATION_FIRST_SHAPE && !holds_answer(validation);
}

static weivel_status refuse_call(weivel_standard_validation *validation, const char *detail, const char *fix) {
    refusal_texts texts = {NULL, NULL, NULL};
    texts.detail = weivel_standard_format_text("%s", detail);
    texts.fix = weivel_standard_format_text("%s", fix);
    texts.path = weivel_standard_format_text("%s", "");
    return weivel_standard_fill_refusal(&validation->refusal, WEIVEL_STANDARD_REFUSAL_MALFORMED_CALL, texts)
               ? WEIVEL_STATUS_MALFORMED
               : WEIVEL_STATUS_OUT_OF_MEMORY;
}

static const weivel_strings *well_known_of(const weivel_standard_validation_options *options) {
    return options == NULL ? NULL : options->well_known_services;
}

static int is_readable(const weivel_strings *ids) {
    uint32_t at;
    if (ids == NULL) {
        return 1;
    }
    if (ids->count > 0 && ids->items == NULL) {
        return 0;
    }
    for (at = 0; at < ids->count; at++) {
        if (ids->items[at] == NULL) {
            return 0;
        }
    }
    return 1;
}

static weivel_status check_call(const char *json, const weivel_standard_validation_options *options,
                                weivel_standard_validation *validation) {
    if (!is_usable(validation)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    if (json == NULL) {
        return refuse_call(validation, "weivel_standard_validate was given no JSON text", "pass the manifest's text");
    }
    if (options != NULL && options->size < OPTIONS_FIRST_SHAPE) {
        return refuse_call(validation, "the validation options are smaller than their first shape",
                           "set the options' size, as WEIVEL_INIT does");
    }
    if (!is_readable(well_known_of(options))) {
        return refuse_call(validation, "the well-known service list holds a NULL id, or no items for its count",
                           "list every well-known id as a string");
    }
    return WEIVEL_STATUS_OK;
}

weivel_status weivel_standard_validate(const char *json, size_t length,
                                       const weivel_standard_validation_options *options,
                                       weivel_standard_validation *validation) {
    weivel_json_error error = {0};
    weivel_json_value *root;
    validating state;
    weivel_status status = check_call(json, options, validation);
    if (status != WEIVEL_STATUS_OK) {
        return status;
    }
    memset(&state, 0, sizeof state);
    state.well_known = well_known_of(options);
    root = weivel_json_parse(json, length, &error);
    if (root == NULL) {
        if (error.out_of_memory) {
            return WEIVEL_STATUS_OUT_OF_MEMORY;
        }
        record_malformed_json(&state, &error);
        return finish(&state, validation);
    }
    check_document(&state, root);
    weivel_json_free(root);
    return finish(&state, validation);
}

void weivel_standard_validation_release(weivel_standard_validation *validation) {
    uint32_t size;
    found_paths found;
    if (validation == NULL || validation->size < VALIDATION_FIRST_SHAPE) {
        return;
    }
    size = validation->size;
    found.items = validation->storage;
    found.count = validation->paths.count;
    found.capacity = found.count;
    release_found(&found);
    weivel_standard_refusal_release(&validation->refusal);
    memset(validation, 0, VALIDATION_FIRST_SHAPE);
    validation->size = size;
}

static weivel_status refuse_unreadable(weivel_standard_validation *validation, weivel_standard_refusal *refusal) {
    char *path = weivel_standard_format_text("%s", refusal->path);
    char **items = weivel_standard_memory_allocate(sizeof *items);
    if (path == NULL || items == NULL) {
        weivel_standard_memory_release(path);
        weivel_standard_memory_release(items);
        weivel_standard_refusal_release(refusal);
        validation->valid = 0;
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    items[0] = path;
    validation->valid = 0;
    validation->paths.items = (const char *const *)items;
    validation->paths.count = 1;
    validation->refusal = *refusal;
    validation->storage = items;
    return WEIVEL_STATUS_REFUSED;
}

weivel_status weivel_standard_load_manifest(const char *json, size_t length,
                                            const weivel_standard_validation_options *options,
                                            weivel_standard_manifest **manifest,
                                            weivel_standard_validation *validation) {
    weivel_standard_refusal refusal = {0};
    weivel_status status;
    if (manifest != NULL) {
        *manifest = NULL;
    }
    if (manifest == NULL) {
        return is_usable(validation) ? refuse_call(validation, "weivel_standard_load_manifest was given no place to "
                                                               "put the manifest",
                                                   "pass the address of a weivel_standard_manifest pointer")
                                     : WEIVEL_STATUS_MALFORMED;
    }
    status = weivel_standard_validate(json, length, options, validation);
    if (status != WEIVEL_STATUS_OK) {
        return status;
    }
    status = weivel_standard_read_manifest(json, length, manifest, &refusal);
    if (status == WEIVEL_STATUS_REFUSED) {
        return refuse_unreadable(validation, &refusal);
    }
    if (status != WEIVEL_STATUS_OK) {
        validation->valid = 0;
    }
    return status;
}
