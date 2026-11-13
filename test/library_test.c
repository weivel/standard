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
    "  {'name': 'repeat', 'symbol': 'echo_repeat', 'kind': 'json', 'argument': {'type': 'string'}, 'result': true},"
    "  {'name': 'where', 'symbol': 'echo_where', 'kind': 'json', 'argument': false, 'result': {},"
    "   'context': ['home', 'arguments']}]},"
    " {'id': 'echo-twice', 'functions': ["
    "  {'name': 'twice', 'symbol': 'echo_twice', 'kind': 'json', 'argument': true, 'result': true,"
    "   'context': ['environment']}]}]}";

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
        CHECK(function->kind == WEIVEL_STANDARD_FUNCTION_JSON);
        CHECK(function->context == 0);
        CHECK(weivel_json_kind_of(function->argument) == WEIVEL_JSON_OBJECT);
        check_text(weivel_json_as_string(weivel_json_object_get(function->argument, "type")), "string", "argument");
        CHECK(weivel_json_kind_of(function->result) == WEIVEL_JSON_BOOL);
        function = &library.services[0].functions[1];
        check_text(function->name, "where", "where name");
        CHECK(function->context == (WEIVEL_STANDARD_CONTEXT_HOME | WEIVEL_STANDARD_CONTEXT_ARGUMENTS));
        CHECK(weivel_json_kind_of(function->argument) == WEIVEL_JSON_BOOL);
        CHECK(weivel_json_kind_of(function->result) == WEIVEL_JSON_OBJECT);
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

static void reads_a_bytes_function(void) {
    weivel_standard_library library = WEIVEL_INIT(weivel_standard_library);
    const char *json = json_of(
        "{'importName': 'echo', 'version': '1', 'release': 'echo_release', 'services': [{'id': 'echo', 'functions': ["
        "{'name': 'echo', 'symbol': 'echo_echo', 'kind': 'bytes', 'context': ['arguments']},"
        " {'name': 'repeat', 'symbol': 'echo_repeat', 'kind': 'json', 'argument': true, 'result': true}]}]}");
    const weivel_standard_library_function *function;
    CHECK(weivel_standard_read_library(json, strlen(json), &library, NULL) == WEIVEL_STATUS_OK);
    CHECK(library.service_count == 1 && library.services[0].function_count == 2);
    if (library.service_count == 1 && library.services[0].function_count == 2) {
        function = &library.services[0].functions[0];
        CHECK(function->kind == WEIVEL_STANDARD_FUNCTION_BYTES);
        CHECK(function->argument == NULL && function->result == NULL);
        CHECK(function->context == WEIVEL_STANDARD_CONTEXT_ARGUMENTS);
        CHECK(library.services[0].functions[1].kind == WEIVEL_STANDARD_FUNCTION_JSON);
    }
    weivel_standard_library_release(&library);
}

typedef struct refused_case {
    const char *name;
    const char *json;
    uint32_t code;
    const char *path;
} refused_case;

#define ONE_FUNCTION(extra) \
    "{'importName': 'echo', 'version': '1', 'release': 'echo_release', 'services': [{'id': 'echo', 'functions': [" \
    "{'name': 'repeat', 'symbol': 'echo_repeat', 'kind': 'json', 'argument': true, 'result': true}, " extra "]}]}"

#define WITH_FUNCTION(fields) ONE_FUNCTION("{'name': 'second', 'symbol': 'echo_second', " fields "}")

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
    {"no release", "{'importName': 'echo', 'version': '1', 'services': []}", WEIVEL_STANDARD_REFUSAL_MISSING_FIELD,
     "release"},
    {"release not an identifier", "{'importName': 'echo', 'version': '1', 'release': '9free', 'services': []}",
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "release"},
    {"no services", "{'importName': 'echo', 'version': '1', 'release': 'r'}", WEIVEL_STANDARD_REFUSAL_MISSING_FIELD,
     "services"},
    {"empty services", "{'importName': 'echo', 'version': '1', 'release': 'r', 'services': []}",
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "services"},
    {"services an object", "{'importName': 'echo', 'version': '1', 'release': 'r', 'services': {}}",
     WEIVEL_STANDARD_REFUSAL_WRONG_TYPE, "services"},
    {"a service id with a dot",
     "{'importName': 'echo', 'version': '1', 'release': 'r', 'services': [{'id': 'a.b', 'functions': []}]}",
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "services[0].id"},
    {"a service with no functions",
     "{'importName': 'echo', 'version': '1', 'release': 'r', 'services': [{'id': 'echo'}]}",
     WEIVEL_STANDARD_REFUSAL_MISSING_FIELD, "services[0].functions"},
    {"a function name with a capital start",
     ONE_FUNCTION("{'name': 'Second', 'symbol': 's', 'kind': 'json', 'argument': true, 'result': true}"),
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "services[0].functions[1].name"},
    {"a function symbol with a hyphen",
     ONE_FUNCTION("{'name': 'second', 'symbol': 'echo-second', 'kind': 'json', 'argument': true,"
                  " 'result': true}"),
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "services[0].functions[1].symbol"},
    {"a function of another kind", WITH_FUNCTION("'kind': 'binary', 'argument': true, 'result': true"),
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "services[0].functions[1].kind"},
    {"a bytes function with an argument", WITH_FUNCTION("'kind': 'bytes', 'argument': true"),
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "services[0].functions[1].argument"},
    {"a bytes function with a result", WITH_FUNCTION("'kind': 'bytes', 'result': {}"),
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "services[0].functions[1].result"},
    {"a kind in another case", WITH_FUNCTION("'kind': 'Bytes'"), WEIVEL_STANDARD_REFUSAL_INVALID_VALUE,
     "services[0].functions[1].kind"},
    {"a function with no result", WITH_FUNCTION("'kind': 'json', 'argument': true"),
     WEIVEL_STANDARD_REFUSAL_MISSING_FIELD, "services[0].functions[1].result"},
    {"an argument that is a string", WITH_FUNCTION("'kind': 'json', 'argument': 'any', 'result': true"),
     WEIVEL_STANDARD_REFUSAL_WRONG_TYPE, "services[0].functions[1].argument"},
    {"a context fact named twice",
     WITH_FUNCTION("'kind': 'json', 'argument': true, 'result': true, 'context': ['home', 'environment', 'home']"),
     WEIVEL_STANDARD_REFUSAL_DUPLICATE, "services[0].functions[1].context[2]"},
    {"an unknown context fact", WITH_FUNCTION("'kind': 'json', 'argument': true, 'result': true, 'context': ['clock']"),
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "services[0].functions[1].context[0]"},
    {"an empty context", WITH_FUNCTION("'kind': 'json', 'argument': true, 'result': true, 'context': []"),
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "services[0].functions[1].context"},
    {"a function that is not an object", ONE_FUNCTION("3"), WEIVEL_STANDARD_REFUSAL_WRONG_TYPE,
     "services[0].functions[1]"},
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
    weivel_standard_refusal_release(&refusal);
}

static void ignores_fields_it_does_not_define(void) {
    weivel_standard_library library = WEIVEL_INIT(weivel_standard_library);
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    const char *json = json_of(
        "{'$schema': 7, 'site': {'x': 1}, 'importName': 'echo', 'version': '1', 'release': 'r', 'services': ["
        "{'id': 'echo', 'later': [], 'functions': [{'name': 'a', 'symbol': 'a', 'kind': 'json', 'argument': true,"
        " 'result': true, 'provider': 'x'}]}]}");
    CHECK(weivel_standard_read_library(json, strlen(json), &library, &refusal) == WEIVEL_STATUS_OK);
    CHECK(refusal.storage == NULL);
    CHECK(library.service_count == 1 && library.services[0].function_count == 1);
    weivel_standard_library_release(&library);
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
    reads_a_bytes_function();
    for (at = 0; at < sizeof REFUSED / sizeof REFUSED[0]; at++) {
        refuses_by_path(&REFUSED[at]);
    }
    ignores_fields_it_does_not_define();
    refuses_a_nul_in_a_name();
    refuses_without_a_refusal_to_fill();
    refuses_a_malformed_call();
    return check_summary("library_test");
}
