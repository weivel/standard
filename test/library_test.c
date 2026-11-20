#include "check.h"
#include "weivel/library.h"

#include <stdlib.h>

#define TEXT_CAPACITY 4096

/** Single quotes stand for double quotes, so the declarations read as JSON on the page. */
static const char *json_of(const char *quoted) {
    static char text[TEXT_CAPACITY];
    size_t at;
    for (at = 0; quoted[at] != '\0' && at + 1 < TEXT_CAPACITY; at++) {
        text[at] = quoted[at] == '\'' ? '"' : quoted[at];
    }
    text[at] = '\0';
    return text;
}

static const char *const ECHO_DECLARATION =
    "{'$schema': 'declaration.schema.json', 'importName': 'echo', 'version': '0.1.0', 'release': 'echo_release',"
    " 'services': [{'id': 'echo', 'functions': ["
    "  {'name': 'repeat', 'symbol': 'echo_repeat', 'kind': 'bytes'},"
    "  {'name': 'where', 'symbol': 'echo_where', 'kind': 'bytes', 'context': ['home', 'arguments']}]},"
    " {'id': 'echo-twice', 'functions': ["
    "  {'name': 'twice', 'symbol': 'echo_twice', 'kind': 'bytes', 'context': ['environment']}]}]}";

static void reads_a_declaration(void) {
    weivel_standard_library library = WEIVEL_INIT(weivel_standard_library);
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    const char *json = json_of(ECHO_DECLARATION);
    const weivel_standard_library_function *function;
    CHECK(weivel_standard_read_library(json, strlen(json), &library, &refusal) == WEIVEL_STATUS_OK);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_NONE && refusal.storage == NULL);
    check_text(library.import_name, "echo", "importName");
    check_text(library.version, "0.1.0", "version");
    check_text(library.release, "echo_release", "release");
    CHECK(library.service_count == 2);
    if (library.service_count == 2) {
        check_text(library.services[0].id, "echo", "services[0].id");
        CHECK(library.services[0].function_count == 2);
        function = &library.services[0].functions[0];
        check_text(function->name, "repeat", "repeat name");
        check_text(function->symbol, "echo_repeat", "repeat symbol");
        CHECK(function->kind == WEIVEL_STANDARD_FUNCTION_BYTES);
        CHECK(function->context == 0);
        function = &library.services[0].functions[1];
        check_text(function->name, "where", "where name");
        CHECK(function->kind == WEIVEL_STANDARD_FUNCTION_BYTES);
        CHECK(function->context == (WEIVEL_STANDARD_CONTEXT_HOME | WEIVEL_STANDARD_CONTEXT_ARGUMENTS));
        check_text(library.services[1].id, "echo-twice", "services[1].id");
        CHECK(library.services[1].function_count == 1);
        CHECK(library.services[1].functions[0].context == WEIVEL_STANDARD_CONTEXT_ENVIRONMENT);
    }
    weivel_standard_library_release(&library);
    CHECK(library.import_name == NULL && library.services == NULL && library.service_count == 0 &&
          library.storage == NULL);
    weivel_standard_library_release(&library);
    weivel_standard_library_release(NULL);
}

/** `argument` and `result` left the format with the json kind, so they are fields it does not define. */
static void ignores_a_former_schema_field(void) {
    weivel_standard_library library = WEIVEL_INIT(weivel_standard_library);
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    const char *json = json_of(
        "{'importName': 'echo', 'version': '1', 'release': 'echo_release', 'services': [{'id': 'echo', 'functions': ["
        "{'name': 'echo', 'symbol': 'echo_echo', 'kind': 'bytes', 'argument': true, 'result': {}}]}]}");
    CHECK(weivel_standard_read_library(json, strlen(json), &library, &refusal) == WEIVEL_STATUS_OK);
    CHECK(refusal.storage == NULL);
    CHECK(library.service_count == 1 && library.services[0].function_count == 1);
    if (library.service_count == 1 && library.services[0].function_count == 1) {
        CHECK(library.services[0].functions[0].kind == WEIVEL_STANDARD_FUNCTION_BYTES);
    }
    weivel_standard_library_release(&library);
    weivel_standard_refusal_release(&refusal);
}

typedef struct refused_case {
    const char *name;
    const char *json;
    uint32_t code;
    const char *path;
} refused_case;

#define ONE_FUNCTION(extra) \
    "{'importName': 'echo', 'version': '1', 'release': 'echo_release', 'services': [{'id': 'echo', 'functions': [" \
    "{'name': 'repeat', 'symbol': 'echo_repeat', 'kind': 'bytes'}, " extra "]}]}"

#define WITH_FUNCTION(fields) ONE_FUNCTION("{'name': 'second', 'symbol': 'echo_second', " fields "}")

#define DESCRIBED(description) \
    "{'importName': 'echo', 'version': '1', 'services': [{'id': 'echo', 'description': " description "," \
    " 'functions': [{'name': 'definition', 'symbol': 'echo_definition', 'kind': 'definition'}]}]}"

static const refused_case REFUSED[] = {
    {"not json", "{'importName': ", WEIVEL_STANDARD_REFUSAL_MALFORMED_JSON, ""},
    {"not an object", "['echo']", WEIVEL_STANDARD_REFUSAL_WRONG_TYPE, ""},
    {"no importName", "{'version': '1', 'release': 'r', 'services': []}", WEIVEL_STANDARD_REFUSAL_MISSING_FIELD,
     "importName"},
    {"importName with a capital", "{'importName': 'Echo', 'version': '1', 'release': 'r', 'services': []}",
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "importName"},
    {"importName with an underscore", "{'importName': 'echo_x', 'version': '1', 'release': 'r', 'services': []}",
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "importName"},
    {"importName a number", "{'importName': 7, 'version': '1', 'release': 'r', 'services': []}",
     WEIVEL_STANDARD_REFUSAL_WRONG_TYPE, "importName"},
    {"empty version", "{'importName': 'echo', 'version': '', 'release': 'r', 'services': []}",
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "version"},
    {"no release beside a bytes function",
     "{'importName': 'echo', 'version': '1', 'services': [{'id': 'echo', 'functions': ["
     "{'name': 'repeat', 'symbol': 'echo_repeat', 'kind': 'bytes'}]}]}",
     WEIVEL_STANDARD_REFUSAL_MISSING_FIELD, "release"},
    {"no release beside a service of definitions",
     "{'importName': 'echo', 'version': '1', 'services': ["
     "{'id': 'whole', 'functions': [{'name': 'definition', 'symbol': 'echo_definition', 'kind': 'definition'}]},"
     "{'id': 'echo', 'functions': [{'name': 'repeat', 'symbol': 'echo_repeat', 'kind': 'bytes'}]}]}",
     WEIVEL_STANDARD_REFUSAL_MISSING_FIELD, "release"},
    {"release not an identifier", "{'importName': 'echo', 'version': '1', 'release': '9free', 'services': []}",
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "release"},
    {"no services", "{'importName': 'echo', 'version': '1', 'release': 'r'}", WEIVEL_STANDARD_REFUSAL_MISSING_FIELD,
     "services"},
    {"empty services", "{'importName': 'echo', 'version': '1', 'release': 'r', 'services': []}",
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "services"},
    {"services an object", "{'importName': 'echo', 'version': '1', 'release': 'r', 'services': {}}",
     WEIVEL_STANDARD_REFUSAL_WRONG_TYPE, "services"},
    {"a service id with a doubled dot",
     "{'importName': 'echo', 'version': '1', 'release': 'r', 'services': [{'id': 'a..b', 'functions': []}]}",
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "services[0].id"},
    {"a service id with a leading dot",
     "{'importName': 'echo', 'version': '1', 'release': 'r', 'services': [{'id': '.a', 'functions': []}]}",
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "services[0].id"},
    {"a service id with a trailing dot",
     "{'importName': 'echo', 'version': '1', 'release': 'r', 'services': [{'id': 'a.', 'functions': []}]}",
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "services[0].id"},
    {"a service id with a dot beside a hyphen",
     "{'importName': 'echo', 'version': '1', 'release': 'r', 'services': [{'id': 'a.-b', 'functions': []}]}",
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "services[0].id"},
    {"a service with no functions",
     "{'importName': 'echo', 'version': '1', 'release': 'r', 'services': [{'id': 'echo'}]}",
     WEIVEL_STANDARD_REFUSAL_MISSING_FIELD, "services[0].functions"},
    {"a function name with a capital start", ONE_FUNCTION("{'name': 'Second', 'symbol': 's', 'kind': 'bytes'}"),
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "services[0].functions[1].name"},
    {"a function symbol with a hyphen", ONE_FUNCTION("{'name': 'second', 'symbol': 'echo-second', 'kind': 'bytes'}"),
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "services[0].functions[1].symbol"},
    {"a function of another kind", WITH_FUNCTION("'kind': 'binary'"), WEIVEL_STANDARD_REFUSAL_INVALID_VALUE,
     "services[0].functions[1].kind"},
    {"a json function", WITH_FUNCTION("'kind': 'json', 'argument': true, 'result': true"),
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "services[0].functions[1].kind"},
    {"a json function first in its service",
     "{'importName': 'echo', 'version': '1', 'release': 'r', 'services': [{'id': 'echo', 'functions': ["
     "{'name': 'repeat', 'symbol': 'echo_repeat', 'kind': 'json'}]}]}",
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "services[0].functions[0].kind"},
    {"a function with no kind", WITH_FUNCTION("'context': ['home']"), WEIVEL_STANDARD_REFUSAL_MISSING_FIELD,
     "services[0].functions[1].kind"},
    {"a kind in another case", WITH_FUNCTION("'kind': 'Bytes'"), WEIVEL_STANDARD_REFUSAL_INVALID_VALUE,
     "services[0].functions[1].kind"},
    {"a struct function", WITH_FUNCTION("'kind': 'struct'"), WEIVEL_STANDARD_REFUSAL_INVALID_VALUE,
     "services[0].functions[1].kind"},
    {"a definition after a bytes function", WITH_FUNCTION("'kind': 'definition'"),
     WEIVEL_STANDARD_REFUSAL_CONFLICTING_FIELD, "services[0].functions[1]"},
    {"a bytes function after a definition",
     "{'importName': 'echo', 'version': '1', 'release': 'r', 'services': [{'id': 'echo', 'functions': ["
     "{'name': 'definition', 'symbol': 'echo_definition', 'kind': 'definition'},"
     "{'name': 'repeat', 'symbol': 'echo_repeat', 'kind': 'bytes'}]}]}",
     WEIVEL_STANDARD_REFUSAL_CONFLICTING_FIELD, "services[0].functions[1]"},
    {"a definition third in its service",
     "{'importName': 'echo', 'version': '1', 'release': 'r', 'services': [{'id': 'echo', 'functions': ["
     "{'name': 'a', 'symbol': 'a', 'kind': 'bytes'}, {'name': 'b', 'symbol': 'b', 'kind': 'bytes'},"
     "{'name': 'definition', 'symbol': 'echo_definition', 'kind': 'definition'}]}]}",
     WEIVEL_STANDARD_REFUSAL_CONFLICTING_FIELD, "services[0].functions[2]"},
    {"context facts on a definition",
     "{'importName': 'echo', 'version': '1', 'services': [{'id': 'echo', 'functions': ["
     "{'name': 'definition', 'symbol': 'echo_definition', 'kind': 'definition', 'context': ['home']}]}]}",
     WEIVEL_STANDARD_REFUSAL_CONFLICTING_FIELD, "services[0].functions[0].context"},
    {"an empty context on a definition",
     "{'importName': 'echo', 'version': '1', 'services': [{'id': 'echo', 'functions': ["
     "{'name': 'definition', 'symbol': 'echo_definition', 'kind': 'definition', 'context': []}]}]}",
     WEIVEL_STANDARD_REFUSAL_CONFLICTING_FIELD, "services[0].functions[0].context"},
    {"two definitions in one service",
     "{'importName': 'echo', 'version': '1', 'services': [{'id': 'echo', 'functions': ["
     "{'name': 'definition', 'symbol': 'echo_definition', 'kind': 'definition'},"
     "{'name': 'again', 'symbol': 'echo_again', 'kind': 'definition'}]}]}",
     WEIVEL_STANDARD_REFUSAL_CONFLICTING_FIELD, "services[0].functions[1]"},
    {"a definition beside another function in a later service",
     "{'importName': 'echo', 'version': '1', 'services': ["
     "{'id': 'echo', 'functions': [{'name': 'definition', 'symbol': 'echo_definition', 'kind': 'definition'}]},"
     "{'id': 'other', 'functions': [{'name': 'definition', 'symbol': 'other_definition', 'kind': 'definition'},"
     "{'name': 'repeat', 'symbol': 'echo_repeat', 'kind': 'bytes'}]}]}",
     WEIVEL_STANDARD_REFUSAL_CONFLICTING_FIELD, "services[1].functions[1]"},
    {"a context fact named twice", WITH_FUNCTION("'kind': 'bytes', 'context': ['home', 'environment', 'home']"),
     WEIVEL_STANDARD_REFUSAL_DUPLICATE, "services[0].functions[1].context[2]"},
    {"an unknown context fact", WITH_FUNCTION("'kind': 'bytes', 'context': ['clock']"),
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "services[0].functions[1].context[0]"},
    {"an empty context", WITH_FUNCTION("'kind': 'bytes', 'context': []"), WEIVEL_STANDARD_REFUSAL_INVALID_VALUE,
     "services[0].functions[1].context"},
    {"a function that is not an object", ONE_FUNCTION("3"), WEIVEL_STANDARD_REFUSAL_WRONG_TYPE,
     "services[0].functions[1]"},
    {"a description that is not a string", DESCRIBED("7"), WEIVEL_STANDARD_REFUSAL_WRONG_TYPE,
     "services[0].description"},
    {"an empty description", DESCRIBED("''"), WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "services[0].description"},
    {"a description not a C identifier", DESCRIBED("'echo-description'"), WEIVEL_STANDARD_REFUSAL_INVALID_VALUE,
     "services[0].description"},
    {"a description beside bytes functions",
     "{'importName': 'echo', 'version': '1', 'release': 'r', 'services': [{'id': 'echo', 'description': 'echo_d',"
     " 'functions': [{'name': 'repeat', 'symbol': 'echo_repeat', 'kind': 'bytes'}]}]}",
     WEIVEL_STANDARD_REFUSAL_CONFLICTING_FIELD, "services[0].description"},
};

static void refuses_by_path(const refused_case *item) {
    weivel_standard_library library = WEIVEL_INIT(weivel_standard_library);
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    const char *json = json_of(item->json);
    weivel_status status = weivel_standard_read_library(json, strlen(json), &library, &refusal);
    if (status != WEIVEL_STATUS_REFUSED || refusal.code != item->code) {
        fprintf(stderr, "%s: status %d code %u, wanted refused with %u\n", item->name, (int)status,
                (unsigned)refusal.code, (unsigned)item->code);
        check_failures++;
    }
    check_text(refusal.path, item->path, item->name);
    CHECK(refusal.detail != NULL && refusal.detail[0] != '\0');
    CHECK(refusal.fix != NULL && refusal.fix[0] != '\0');
    CHECK(library.storage == NULL && library.import_name == NULL);
    weivel_standard_library_release(&library);
    weivel_standard_refusal_release(&refusal);
}

static void ignores_fields_it_does_not_define(void) {
    weivel_standard_library library = WEIVEL_INIT(weivel_standard_library);
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    const char *json = json_of(
        "{'$schema': 7, 'site': {'x': 1}, 'importName': 'echo', 'version': '1', 'release': 'r', 'services': ["
        "{'id': 'echo', 'later': [], 'functions': [{'name': 'a', 'symbol': 'a', 'kind': 'bytes', 'provider': 'x'}]}]}");
    CHECK(weivel_standard_read_library(json, strlen(json), &library, &refusal) == WEIVEL_STATUS_OK);
    CHECK(refusal.storage == NULL);
    CHECK(library.service_count == 1 && library.services[0].function_count == 1);
    weivel_standard_library_release(&library);
}

static const char *const DEFINITIONS_ALONE =
    "{'importName': 'echo', 'version': '0.2.0', 'services': ["
    " {'id': 'echo', 'functions': [{'name': 'definition', 'symbol': 'echo_service_definition', 'kind': 'definition'}]},"
    " {'id': 'echo-twice', 'functions': [{'name': 'definition', 'symbol': 'echo_twice_definition',"
    " 'kind': 'definition'}]}]}";

static void reads_a_library_of_definitions_with_no_release(void) {
    weivel_standard_library library = WEIVEL_INIT(weivel_standard_library);
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    const char *json = json_of(DEFINITIONS_ALONE);
    CHECK(weivel_standard_read_library(json, strlen(json), &library, &refusal) == WEIVEL_STATUS_OK);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_NONE && refusal.storage == NULL);
    CHECK(library.release == NULL);
    CHECK(library.service_count == 2);
    if (library.service_count == 2) {
        CHECK(library.services[0].function_count == 1 && library.services[1].function_count == 1);
        check_text(library.services[0].functions[0].name, "definition", "definition name");
        check_text(library.services[0].functions[0].symbol, "echo_service_definition", "definition symbol");
        CHECK(library.services[0].functions[0].kind == WEIVEL_STANDARD_FUNCTION_DEFINITION);
        CHECK(library.services[1].functions[0].kind == WEIVEL_STANDARD_FUNCTION_DEFINITION);
    }
    weivel_standard_library_release(&library);
    weivel_standard_refusal_release(&refusal);
}

static void reads_a_service_id_of_dotted_words(void) {
    weivel_standard_library library = WEIVEL_INIT(weivel_standard_library);
    const char *json = json_of(
        "{'importName': 'stub-host', 'version': '1', 'services': [{'id': 'weivel.host', 'functions': ["
        "{'name': 'definition', 'symbol': 'stub_host_definition', 'kind': 'definition'}]}]}");
    CHECK(weivel_standard_read_library(json, strlen(json), &library, NULL) == WEIVEL_STATUS_OK);
    CHECK(library.service_count == 1);
    if (library.service_count == 1) {
        check_text(library.services[0].id, "weivel.host", "a standard service id");
    }
    weivel_standard_library_release(&library);
}

static void reads_a_release_beside_definitions_alone(void) {
    weivel_standard_library library = WEIVEL_INIT(weivel_standard_library);
    const char *json = json_of(
        "{'importName': 'echo', 'version': '1', 'release': 'echo_release', 'services': [{'id': 'echo', 'functions': ["
        "{'name': 'definition', 'symbol': 'echo_service_definition', 'kind': 'definition'}]}]}");
    CHECK(weivel_standard_read_library(json, strlen(json), &library, NULL) == WEIVEL_STATUS_OK);
    check_text(library.release, "echo_release", "a release beside definitions");
    weivel_standard_library_release(&library);
}

static void reads_a_definition_service_beside_a_bytes_service(void) {
    weivel_standard_library library = WEIVEL_INIT(weivel_standard_library);
    const char *json = json_of(
        "{'importName': 'echo', 'version': '1', 'release': 'echo_release', 'services': ["
        "{'id': 'whole', 'functions': [{'name': 'definition', 'symbol': 'echo_service_definition',"
        " 'kind': 'definition'}]},"
        "{'id': 'echo', 'functions': [{'name': 'echo', 'symbol': 'echo_echo', 'kind': 'bytes'}]}]}");
    CHECK(weivel_standard_read_library(json, strlen(json), &library, NULL) == WEIVEL_STATUS_OK);
    CHECK(library.service_count == 2);
    if (library.service_count == 2) {
        CHECK(library.services[0].functions[0].kind == WEIVEL_STANDARD_FUNCTION_DEFINITION);
        CHECK(library.services[1].functions[0].kind == WEIVEL_STANDARD_FUNCTION_BYTES);
    }
    weivel_standard_library_release(&library);
}

static const char *const DESCRIBED_LIBRARY =
    "{'importName': 'echo', 'version': '1', 'release': 'echo_release', 'services': ["
    "{'id': 'echo', 'description': 'echo_description', 'functions': ["
    " {'name': 'definition', 'symbol': 'echo_definition', 'kind': 'definition'}]},"
    "{'id': 'echo-bytes', 'functions': [{'name': 'echo', 'symbol': 'echo_echo', 'kind': 'bytes'}]}]}";

static void reads_a_description_beside_a_definition(void) {
    weivel_standard_library library = WEIVEL_INIT(weivel_standard_library);
    const char *json = json_of(DESCRIBED_LIBRARY);
    CHECK(weivel_standard_read_library(json, strlen(json), &library, NULL) == WEIVEL_STATUS_OK);
    CHECK(library.service_count == 2 && library.descriptions != NULL);
    if (library.service_count == 2 && library.descriptions != NULL) {
        check_text(library.descriptions[0], "echo_description", "a service's description");
        CHECK(library.descriptions[1] == NULL);
    }
    weivel_standard_library_release(&library);
    CHECK(library.descriptions == NULL && library.storage == NULL);
}

static void reads_no_descriptions_where_none_is_declared(void) {
    weivel_standard_library library = WEIVEL_INIT(weivel_standard_library);
    const char *json = json_of(DEFINITIONS_ALONE);
    CHECK(weivel_standard_read_library(json, strlen(json), &library, NULL) == WEIVEL_STATUS_OK);
    CHECK(library.service_count == 2 && library.descriptions != NULL);
    if (library.service_count == 2 && library.descriptions != NULL) {
        CHECK(library.descriptions[0] == NULL && library.descriptions[1] == NULL);
    }
    weivel_standard_library_release(&library);
}

typedef union first_shape_library {
    weivel_standard_library library;
    unsigned char bytes[sizeof(weivel_standard_library)];
} first_shape_library;

/** A caller compiled before `descriptions` was appended has a declaration ending at `storage`. */
static void fills_a_first_shape_declaration_within_its_size(void) {
    first_shape_library caller;
    weivel_standard_library *library = &caller.library;
    const char *json = json_of(DESCRIBED_LIBRARY);
    size_t first_shape = offsetof(weivel_standard_library, storage) + sizeof(void *);
    size_t at;
    int untouched = 1;
    memset(caller.bytes, 0, first_shape);
    memset(caller.bytes + first_shape, 0xab, sizeof caller.bytes - first_shape);
    library->size = (uint32_t)first_shape;
    CHECK(weivel_standard_read_library(json, strlen(json), library, NULL) == WEIVEL_STATUS_OK);
    check_text(library->import_name, "echo", "a first-shape declaration is read");
    for (at = first_shape; at < sizeof caller.bytes; at++) {
        untouched = untouched && caller.bytes[at] == 0xab;
    }
    CHECK(untouched);
    weivel_standard_library_release(library);
    CHECK(library->storage == NULL && library->services == NULL);
    library->size = (uint32_t)first_shape - 1;
    CHECK(weivel_standard_read_library(json, strlen(json), library, NULL) == WEIVEL_STATUS_MALFORMED);
}

static void refuses_a_nul_in_a_name(void) {
    static const char json[] = "{\"importName\": \"echo\", \"version\": \"1\\u0000\", \"release\": \"r\","
                               " \"services\": []}";
    weivel_standard_library library = WEIVEL_INIT(weivel_standard_library);
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    CHECK(weivel_standard_read_library(json, strlen(json), &library, &refusal) == WEIVEL_STATUS_REFUSED);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_UNREPRESENTABLE);
    check_text(refusal.path, "version", "nul path");
    weivel_standard_refusal_release(&refusal);
}

static void refuses_without_a_refusal_to_fill(void) {
    weivel_standard_library library = WEIVEL_INIT(weivel_standard_library);
    const char *json = json_of("{'importName': 'echo'}");
    CHECK(weivel_standard_read_library(json, strlen(json), &library, NULL) == WEIVEL_STATUS_REFUSED);
    CHECK(library.storage == NULL);
}

static void refuses_a_malformed_call(void) {
    weivel_standard_library library = WEIVEL_INIT(weivel_standard_library);
    weivel_standard_library holding = WEIVEL_INIT(weivel_standard_library);
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    const char *json = json_of(ECHO_DECLARATION);
    CHECK(weivel_standard_read_library(NULL, 0, &library, &refusal) == WEIVEL_STATUS_MALFORMED);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_MALFORMED_CALL);
    weivel_standard_refusal_release(&refusal);
    CHECK(weivel_standard_read_library(json, strlen(json), NULL, &refusal) == WEIVEL_STATUS_MALFORMED);
    weivel_standard_refusal_release(&refusal);
    CHECK(weivel_standard_read_library(json, strlen(json), &holding, NULL) == WEIVEL_STATUS_OK);
    CHECK(weivel_standard_read_library(json, strlen(json), &holding, &refusal) == WEIVEL_STATUS_MALFORMED);
    check_text(holding.import_name, "echo", "a held declaration is left as it was");
    weivel_standard_refusal_release(&refusal);
    weivel_standard_library_release(&holding);
}

int main(void) {
    size_t at;
    reads_a_declaration();
    ignores_a_former_schema_field();
    for (at = 0; at < sizeof REFUSED / sizeof REFUSED[0]; at++) {
        refuses_by_path(&REFUSED[at]);
    }
    ignores_fields_it_does_not_define();
    reads_a_library_of_definitions_with_no_release();
    reads_a_release_beside_definitions_alone();
    reads_a_service_id_of_dotted_words();
    reads_a_definition_service_beside_a_bytes_service();
    reads_a_description_beside_a_definition();
    reads_no_descriptions_where_none_is_declared();
    fills_a_first_shape_declaration_within_its_size();
    refuses_a_nul_in_a_name();
    refuses_without_a_refusal_to_fill();
    refuses_a_malformed_call();
    return check_summary("library_test");
}
