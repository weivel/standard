#include "weivel/library.h"

#include "allocation.h"
#include "report.h"

#include <string.h>

typedef weivel_standard_field_path field_path;
typedef weivel_standard_refusal_texts refusal_texts;

typedef struct checking {
    weivel_standard_refusal *refusal;
    weivel_status status;
} checking;

typedef int (*text_rule)(const char *text, size_t length);

/** A string field and its rule: `broken` completes "<field> ..." when the rule refuses, `fix` is the schema's. */
typedef struct text_field {
    const char *key;
    text_rule rule;
    const char *broken;
    const char *fix;
} text_field;

typedef struct library_storage {
    weivel_json_value *document;
    weivel_standard_library_service *services;
    weivel_standard_library_function *functions;
} library_storage;

typedef struct context_name {
    const char *name;
    uint32_t fact;
} context_name;

static const context_name CONTEXT_NAMES[] = {{"environment", WEIVEL_STANDARD_CONTEXT_ENVIRONMENT},
                                             {"home", WEIVEL_STANDARD_CONTEXT_HOME},
                                             {"arguments", WEIVEL_STANDARD_CONTEXT_ARGUMENTS}};

static const char *const ROOT_FIELDS[] = {"$schema", "importName", "version", "release", "services"};
static const char *const SERVICE_FIELDS[] = {"id", "functions"};
static const char *const FUNCTION_FIELDS[] = {"name", "symbol", "kind", "argument", "result", "context"};

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

static int is_lowercase_or_digit(char byte) {
    return (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9');
}

static int is_letter_or_digit(char byte) {
    return is_lowercase_or_digit(byte) || (byte >= 'A' && byte <= 'Z');
}

/** `^[a-z0-9]+([-.][a-z0-9]+)*$`, or with hyphens alone when `dots` is 0. */
static int matches_words(const char *text, size_t length, int dots) {
    size_t at;
    int after_separator = 1;
    for (at = 0; at < length; at++) {
        if (is_lowercase_or_digit(text[at])) {
            after_separator = 0;
        } else if ((text[at] == '-' || (dots && text[at] == '.')) && !after_separator) {
            after_separator = 1;
        } else {
            return 0;
        }
    }
    return !after_separator;
}

static int matches_import_name(const char *text, size_t length) {
    return matches_words(text, length, 1);
}

static int matches_service_id(const char *text, size_t length) {
    return matches_words(text, length, 0);
}

static int matches_function_name(const char *text, size_t length) {
    size_t at;
    if (length == 0 || text[0] < 'a' || text[0] > 'z') {
        return 0;
    }
    for (at = 1; at < length; at++) {
        if (!is_letter_or_digit(text[at])) {
            return 0;
        }
    }
    return 1;
}

static int matches_c_identifier(const char *text, size_t length) {
    size_t at;
    if (length == 0 || (text[0] >= '0' && text[0] <= '9')) {
        return 0;
    }
    for (at = 0; at < length; at++) {
        if (!is_letter_or_digit(text[at]) && text[at] != '_') {
            return 0;
        }
    }
    return 1;
}

static int is_not_empty(const char *text, size_t length) {
    (void)text;
    return length > 0;
}

static const text_field IMPORT_NAME = {"importName", matches_import_name,
                                       "is not lowercase words joined by single hyphens or dots",
                                       "use lowercase words joined by single hyphens or dots, for example \"echo\""};
static const text_field VERSION = {"version", is_not_empty, "is empty",
                                   "set \"version\" to the library's version, for example \"0.1.0\""};
static const text_field RELEASE = {"release", matches_c_identifier, "is not a C identifier",
                                   "name the exported release function as a C identifier, for example "
                                   "\"echo_release\""};
static const text_field SERVICE_ID = {"id", matches_service_id, "is not lowercase words joined by single hyphens",
                                      "use lowercase words joined by single hyphens, for example \"echo\""};
static const text_field FUNCTION_NAME = {"name", matches_function_name,
                                         "does not start with a lowercase letter and hold letters and digits only",
                                         "start \"name\" with a lowercase letter and use letters and digits only, for "
                                         "example \"repeat\""};
static const text_field FUNCTION_SYMBOL = {"symbol", matches_c_identifier, "is not a C identifier",
                                           "use a C identifier for the exported symbol, for example \"echo_repeat\""};

static const char *subject_of(const char *path) {
    return path[0] == '\0' ? "the declaration" : path;
}

/** `fix` may hold one %s, which names the field. */
static int refuse(checking *state, uint32_t code, const field_path *path, const char *what, const char *fix) {
    refusal_texts texts = {NULL, NULL, NULL};
    if (state->refusal == NULL) {
        state->status = WEIVEL_STATUS_REFUSED;
        return 0;
    }
    texts.path = weivel_standard_path_text(path);
    if (texts.path != NULL) {
        texts.detail = weivel_standard_format_text("%s %s", subject_of(texts.path), what);
        texts.fix = weivel_standard_format_text(fix, subject_of(texts.path));
    }
    state->status =
        weivel_standard_fill_refusal(state->refusal, code, texts) ? WEIVEL_STATUS_REFUSED : WEIVEL_STATUS_OUT_OF_MEMORY;
    return 0;
}

static int refuse_wrong_type(checking *state, const weivel_json_value *value, const field_path *path,
                             const char *expected) {
    refusal_texts texts = {NULL, NULL, NULL};
    if (state->refusal == NULL) {
        state->status = WEIVEL_STATUS_REFUSED;
        return 0;
    }
    texts.path = weivel_standard_path_text(path);
    if (texts.path != NULL) {
        texts.detail = weivel_standard_format_text("%s is %s, where %s is needed", subject_of(texts.path),
                                                   weivel_standard_kind_phrase(value), expected);
        texts.fix = weivel_standard_format_text("write %s as %s", subject_of(texts.path), expected);
    }
    state->status = weivel_standard_fill_refusal(state->refusal, WEIVEL_STANDARD_REFUSAL_WRONG_TYPE, texts)
                        ? WEIVEL_STATUS_REFUSED
                        : WEIVEL_STATUS_OUT_OF_MEMORY;
    return 0;
}

static int refuse_missing(checking *state, const field_path *path) {
    return refuse(state, WEIVEL_STANDARD_REFUSAL_MISSING_FIELD, path, "is missing, and a declaration requires it",
                  "add %s");
}

static int is_known(const char *key, const char *const *known, size_t known_count) {
    size_t at;
    for (at = 0; at < known_count; at++) {
        if (strcmp(key, known[at]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int check_object(checking *state, const weivel_json_value *value, const field_path *path,
                        const char *const *known, size_t known_count) {
    size_t at;
    if (weivel_json_kind_of(value) != WEIVEL_JSON_OBJECT) {
        return refuse_wrong_type(state, value, path, "an object");
    }
    for (at = 0; at < weivel_json_object_size(value); at++) {
        const char *key = weivel_json_object_key_at(value, at);
        if (!is_known(key, known, known_count)) {
            field_path member = {path, key, 0};
            return refuse(state, WEIVEL_STANDARD_REFUSAL_UNKNOWN_FIELD, &member,
                          "is not a field a library declaration defines", "remove %s");
        }
    }
    return 1;
}

static int check_string(checking *state, const weivel_json_value *value, const field_path *path) {
    const char *text = weivel_json_as_string(value);
    if (text == NULL) {
        return refuse_wrong_type(state, value, path, "a string");
    }
    if (strlen(text) != weivel_json_string_length(value)) {
        return refuse(state, WEIVEL_STANDARD_REFUSAL_UNREPRESENTABLE, path,
                      "holds a NUL character, which a C string cannot carry", "remove the NUL character from %s");
    }
    return 1;
}

static int check_text_field(checking *state, const weivel_json_value *object, const field_path *parent,
                            const text_field *field) {
    const weivel_json_value *value = weivel_json_object_get(object, field->key);
    field_path path = {parent, field->key, 0};
    if (value == NULL) {
        return refuse_missing(state, &path);
    }
    if (!check_string(state, value, &path)) {
        return 0;
    }
    if (!field->rule(weivel_json_as_string(value), weivel_json_string_length(value))) {
        return refuse(state, WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, &path, field->broken, field->fix);
    }
    return 1;
}

typedef int (*item_check)(checking *state, const weivel_json_value *item, const field_path *path);

/** A required array of at least one item, each passing `check`. */
static int check_list(checking *state, const weivel_json_value *object, const field_path *parent, const char *key,
                      item_check check) {
    const weivel_json_value *value = weivel_json_object_get(object, key);
    field_path path = {parent, key, 0};
    size_t at;
    if (value == NULL) {
        return refuse_missing(state, &path);
    }
    if (weivel_json_kind_of(value) != WEIVEL_JSON_ARRAY) {
        return refuse_wrong_type(state, value, &path, "an array");
    }
    if (weivel_json_array_length(value) == 0) {
        return refuse(state, WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, &path, "is empty, and needs at least one item",
                      "list at least one item in %s");
    }
    for (at = 0; at < weivel_json_array_length(value); at++) {
        field_path item = {&path, NULL, at};
        if (!check(state, weivel_json_array_at(value, at), &item)) {
            return 0;
        }
    }
    return 1;
}

static int check_kind(checking *state, const weivel_json_value *function, const field_path *parent) {
    const weivel_json_value *value = weivel_json_object_get(function, "kind");
    field_path path = {parent, "kind", 0};
    if (value == NULL) {
        return refuse_missing(state, &path);
    }
    if (!check_string(state, value, &path)) {
        return 0;
    }
    if (strcmp(weivel_json_as_string(value), "json") != 0) {
        return refuse(state, WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, &path, "is not \"json\"",
                      "set \"kind\" to \"json\", the only kind there is");
    }
    return 1;
}

static int check_schema_value(checking *state, const weivel_json_value *function, const field_path *parent,
                              const char *key) {
    const weivel_json_value *value = weivel_json_object_get(function, key);
    field_path path = {parent, key, 0};
    weivel_json_kind kind = weivel_json_kind_of(value);
    if (value == NULL) {
        return refuse_missing(state, &path);
    }
    if (kind != WEIVEL_JSON_OBJECT && kind != WEIVEL_JSON_BOOL) {
        return refuse_wrong_type(state, value, &path, "a JSON Schema, an object or a boolean");
    }
    return 1;
}

static uint32_t context_fact_of(const char *name) {
    size_t at;
    for (at = 0; at < COUNT_OF(CONTEXT_NAMES); at++) {
        if (strcmp(name, CONTEXT_NAMES[at].name) == 0) {
            return CONTEXT_NAMES[at].fact;
        }
    }
    return 0;
}

static int check_context(checking *state, const weivel_json_value *function, const field_path *parent) {
    const weivel_json_value *value = weivel_json_object_get(function, "context");
    field_path path = {parent, "context", 0};
    uint32_t seen = 0;
    size_t at;
    if (value == NULL) {
        return 1;
    }
    if (weivel_json_kind_of(value) != WEIVEL_JSON_ARRAY) {
        return refuse_wrong_type(state, value, &path, "an array");
    }
    if (weivel_json_array_length(value) == 0) {
        return refuse(state, WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, &path, "is empty",
                      "list the facts the function reads, or remove %s");
    }
    for (at = 0; at < weivel_json_array_length(value); at++) {
        field_path item = {&path, NULL, at};
        uint32_t fact;
        if (!check_string(state, weivel_json_array_at(value, at), &item)) {
            return 0;
        }
        fact = context_fact_of(weivel_json_as_string(weivel_json_array_at(value, at)));
        if (fact == 0) {
            return refuse(state, WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, &item,
                          "is not \"environment\", \"home\" or \"arguments\"",
                          "list only \"environment\", \"home\" and \"arguments\" in \"context\"");
        }
        if ((seen & fact) != 0) {
            return refuse(state, WEIVEL_STANDARD_REFUSAL_DUPLICATE, &item, "repeats a fact listed before it",
                          "list each fact once in \"context\"");
        }
        seen |= fact;
    }
    return 1;
}

static int check_function(checking *state, const weivel_json_value *function, const field_path *path) {
    return check_object(state, function, path, FUNCTION_FIELDS, COUNT_OF(FUNCTION_FIELDS)) &&
           check_text_field(state, function, path, &FUNCTION_NAME) &&
           check_text_field(state, function, path, &FUNCTION_SYMBOL) && check_kind(state, function, path) &&
           check_schema_value(state, function, path, "argument") &&
           check_schema_value(state, function, path, "result") && check_context(state, function, path);
}

static int check_service(checking *state, const weivel_json_value *service, const field_path *path) {
    return check_object(state, service, path, SERVICE_FIELDS, COUNT_OF(SERVICE_FIELDS)) &&
           check_text_field(state, service, path, &SERVICE_ID) &&
           check_list(state, service, path, "functions", check_function);
}

static int check_schema_pointer(checking *state, const weivel_json_value *root) {
    const weivel_json_value *value = weivel_json_object_get(root, "$schema");
    field_path path = {NULL, "$schema", 0};
    return value == NULL || weivel_json_kind_of(value) == WEIVEL_JSON_STRING ||
           refuse_wrong_type(state, value, &path, "a string");
}

static int check_library(checking *state, const weivel_json_value *root) {
    return check_object(state, root, NULL, ROOT_FIELDS, COUNT_OF(ROOT_FIELDS)) && check_schema_pointer(state, root) &&
           check_text_field(state, root, NULL, &IMPORT_NAME) && check_text_field(state, root, NULL, &VERSION) &&
           check_text_field(state, root, NULL, &RELEASE) && check_list(state, root, NULL, "services", check_service);
}

static uint32_t context_of(const weivel_json_value *function) {
    const weivel_json_value *value = weivel_json_object_get(function, "context");
    uint32_t facts = 0;
    size_t at;
    for (at = 0; at < weivel_json_array_length(value); at++) {
        facts |= context_fact_of(weivel_json_as_string(weivel_json_array_at(value, at)));
    }
    return facts;
}

static const char *text_of(const weivel_json_value *object, const char *key) {
    return weivel_json_as_string(weivel_json_object_get(object, key));
}

static void fill_function(weivel_standard_library_function *function, const weivel_json_value *value) {
    function->name = text_of(value, "name");
    function->symbol = text_of(value, "symbol");
    function->kind = WEIVEL_STANDARD_FUNCTION_JSON;
    function->context = context_of(value);
    function->argument = weivel_json_object_get(value, "argument");
    function->result = weivel_json_object_get(value, "result");
}

static size_t count_functions(const weivel_json_value *services) {
    size_t total = 0;
    size_t at;
    for (at = 0; at < weivel_json_array_length(services); at++) {
        total += weivel_json_array_length(weivel_json_object_get(weivel_json_array_at(services, at), "functions"));
    }
    return total;
}

static void fill_services(library_storage *storage, const weivel_json_value *services) {
    weivel_standard_library_function *next = storage->functions;
    size_t at;
    for (at = 0; at < weivel_json_array_length(services); at++) {
        const weivel_json_value *service = weivel_json_array_at(services, at);
        const weivel_json_value *functions = weivel_json_object_get(service, "functions");
        size_t index;
        storage->services[at].id = text_of(service, "id");
        storage->services[at].functions = next;
        storage->services[at].function_count = weivel_json_array_length(functions);
        for (index = 0; index < weivel_json_array_length(functions); index++) {
            fill_function(next++, weivel_json_array_at(functions, index));
        }
    }
}

static void release_storage(library_storage *storage) {
    if (storage == NULL) {
        return;
    }
    weivel_json_free(storage->document);
    weivel_standard_memory_release(storage->services);
    weivel_standard_memory_release(storage->functions);
    weivel_standard_memory_release(storage);
}

/** Takes `document`, which has passed check_library, whatever it answers. */
static weivel_status build_library(weivel_json_value *document, weivel_standard_library *library) {
    const weivel_json_value *services = weivel_json_object_get(document, "services");
    size_t service_count = weivel_json_array_length(services);
    library_storage *storage = weivel_standard_memory_allocate_zeroed(1, sizeof *storage);
    if (storage == NULL) {
        weivel_json_free(document);
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    storage->document = document;
    storage->services = weivel_standard_memory_allocate_zeroed(service_count, sizeof *storage->services);
    storage->functions = weivel_standard_memory_allocate_zeroed(count_functions(services), sizeof *storage->functions);
    if (storage->services == NULL || storage->functions == NULL) {
        release_storage(storage);
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    fill_services(storage, services);
    library->import_name = text_of(document, "importName");
    library->version = text_of(document, "version");
    library->release = text_of(document, "release");
    library->services = storage->services;
    library->service_count = service_count;
    library->storage = storage;
    return WEIVEL_STATUS_OK;
}

static weivel_status refuse_malformed_call(const char *json, const weivel_standard_library *library,
                                           weivel_standard_refusal *refusal) {
    if (library == NULL) {
        return weivel_standard_refuse_call(refusal, "weivel_standard_read_library was given no declaration to fill",
                                           "pass the address of an empty weivel_standard_library");
    }
    if (json == NULL) {
        return weivel_standard_refuse_call(refusal, "weivel_standard_read_library was given no JSON text",
                                           "pass the declaration's text");
    }
    return weivel_standard_refuse_call(refusal,
                                       "weivel_standard_read_library was given a declaration still holding one",
                                       "release the declaration first, or pass an empty one");
}

weivel_status weivel_standard_read_library(const char *json, size_t length, weivel_standard_library *library,
                                           weivel_standard_refusal *refusal) {
    weivel_json_error error = {0};
    weivel_json_value *document;
    checking state;
    if (weivel_standard_refusal_holds_one(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    if (json == NULL || library == NULL || library->storage != NULL) {
        return refuse_malformed_call(json, library, refusal);
    }
    document = weivel_json_parse(json, length, &error);
    if (document == NULL) {
        return error.out_of_memory ? WEIVEL_STATUS_OUT_OF_MEMORY
                                   : weivel_standard_refuse_malformed_json(refusal, &error);
    }
    state.refusal = refusal;
    state.status = WEIVEL_STATUS_OK;
    if (!check_library(&state, document)) {
        weivel_json_free(document);
        return state.status;
    }
    return build_library(document, library);
}

void weivel_standard_library_release(weivel_standard_library *library) {
    weivel_standard_library empty = {0};
    if (library == NULL) {
        return;
    }
    release_storage(library->storage);
    *library = empty;
}
