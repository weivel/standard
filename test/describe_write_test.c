#include <stdio.h>
#include <string.h>

#include "allocation.h"
#include "check.h"
#include "files.h"
#include "every_form.h"
#include "nested.h"
#include "platform.h"
#include "reader.h"
#include "writer.h"

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))
#define FIXTURE(name) WEIVEL_DESCRIBE_FIXTURES "/" name

typedef struct golden_case {
    /** The golden file's name under test/describe/golden, and the name its actual output is kept under. */
    const char *golden;
    const char *const *paths;
    const char *const *includes;
    size_t header_count;
    describe_names names;
    describe_service service;
} golden_case;

static void keep_actual(const char *golden, const describe_text *source) {
    char path[512];
    snprintf(path, sizeof path, "%s/%s", WEIVEL_DESCRIBE_ACTUAL, golden);
    if (describe_write_file(path, source->bytes, source->length)) {
        fprintf(stderr, "%s: the source written is kept at %s\n", golden, path);
    }
}

static uint32_t first_differing_line(const char *actual, size_t actual_length, const char *expected,
                                     size_t expected_length) {
    uint32_t line = 1;
    size_t at;
    for (at = 0; at < actual_length && at < expected_length && actual[at] == expected[at]; at++) {
        if (actual[at] == '\n') {
            line++;
        }
    }
    return line;
}

static void compare_with_golden(const char *golden, const describe_text *source) {
    char path[512];
    char *expected = NULL;
    size_t length = 0;
    snprintf(path, sizeof path, "%s/golden/%s", WEIVEL_DESCRIBE_FIXTURES, golden);
    if (weivel_standard_files_read(path, &expected, &length) != WEIVEL_STANDARD_FILES_DONE) {
        fprintf(stderr, "%s cannot be read\n", path);
        check_failures++;
        keep_actual(golden, source);
        return;
    }
    if (length != source->length || memcmp(expected, source->bytes, length) != 0) {
        fprintf(stderr, "%s: the source written differs from the golden file from line %u\n", golden,
                (unsigned)first_differing_line(source->bytes, source->length, expected, length));
        check_failures++;
        keep_actual(golden, source);
    }
    weivel_standard_memory_release(expected);
}

static void writes_the_golden_source(const golden_case *test) {
    describe_model model;
    describe_refusal refusal;
    describe_text source;
    describe_service service = test->service;
    if (describe_read(&model, test->paths, test->header_count, &test->names, &refusal) != DESCRIBE_DONE) {
        fprintf(stderr, "%s: the headers were not read: %s\n", test->golden,
                refusal.message == NULL ? "out of memory" : refusal.message);
        describe_refusal_release(&refusal);
        check_failures++;
        return;
    }
    service.includes = test->includes;
    service.include_count = test->header_count;
    if (describe_write(&model, &service, &source, &refusal) != DESCRIBE_DONE) {
        fprintf(stderr, "%s: nothing was written: %s\n", test->golden,
                refusal.message == NULL ? "out of memory" : refusal.message);
        describe_refusal_release(&refusal);
        check_failures++;
    } else {
        CHECK(source.bytes[source.length] == '\0');
        compare_with_golden(test->golden, &source);
        describe_text_release(&source);
    }
    describe_model_release(&model);
}

static describe_service service_of(const char *definition, const char *symbol, const char *prefix,
                                   const char *export_macro) {
    describe_service service;
    memset(&service, 0, sizeof service);
    service.definition = definition;
    service.symbol = symbol;
    service.prefix = prefix;
    service.export_macro = export_macro;
    return service;
}

const weivel_standard_description *nested_description_of(void);

static const weivel_standard_described_member REMOTE_MEMBERS[] = {
    {.name = "size",
     .offset = offsetof(probe_remote, size),
     .size = sizeof(((probe_remote *)0)->size),
     .kind = WEIVEL_STANDARD_DESCRIBED_U32,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL},
    {.name = "context",
     .offset = offsetof(probe_remote, context),
     .size = sizeof(((probe_remote *)0)->context),
     .kind = WEIVEL_STANDARD_DESCRIBED_CONTEXT,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL}};

static const weivel_standard_described_type REMOTE_TYPES[] = {
    {.name = "probe_remote",
     .size = sizeof(probe_remote),
     .first_shape = offsetof(probe_remote, ping) + sizeof(((probe_remote *)0)->ping),
     .members = REMOTE_MEMBERS,
     .member_count = COUNT_OF(REMOTE_MEMBERS)}};

static const weivel_standard_described_parameter PING_PARAMETERS[] = {
    {.name = "context",
     .kind = WEIVEL_STANDARD_DESCRIBED_CONTEXT,
     .role = WEIVEL_STANDARD_DESCRIBED_INPUT,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL},
    {.name = "pong",
     .kind = WEIVEL_STANDARD_DESCRIBED_U64,
     .role = WEIVEL_STANDARD_DESCRIBED_ANSWER,
     .element = WEIVEL_STANDARD_DESCRIBED_NONE,
     .type = NULL,
     .count = NULL,
     .release = NULL}};

static const weivel_standard_described_operation REMOTE_OPERATIONS[] = {
    {.name = "ping",
     .offset = offsetof(probe_remote, ping),
     .returns = WEIVEL_STANDARD_DESCRIBED_STATUS,
     .parameters = PING_PARAMETERS,
     .parameter_count = COUNT_OF(PING_PARAMETERS)}};

static const weivel_standard_description REMOTE = {.size = (uint32_t)sizeof(weivel_standard_description),
                                                    .definition = "probe_remote",
                                                    .types = REMOTE_TYPES,
                                                    .type_count = COUNT_OF(REMOTE_TYPES),
                                                    .enums = NULL,
                                                    .enum_count = 0,
                                                    .operations = REMOTE_OPERATIONS,
                                                    .operation_count = COUNT_OF(REMOTE_OPERATIONS),
                                                    .blocking = NULL,
                                                    .blocking_count = 0,
                                                    .definitions = NULL,
                                                    .definition_count = 0};

/** The description another library would export for probe_remote, which golden/nested.c calls. */
const weivel_standard_description *probe_remote_description(void);

const weivel_standard_description *probe_remote_description(void) {
    return &REMOTE;
}

static void writes_each_golden_source(void) {
    static const char *const services[] = {WEIVEL_DESCRIBE_SERVICES_HEADER};
    static const char *const services_include[] = {"weivel/services.h"};
    static const char *const probe[] = {WEIVEL_DESCRIBE_SERVICES_HEADER, FIXTURE("every_form.h")};
    static const char *const probe_includes[] = {"weivel/services.h", "every_form.h"};
    static const char *const restated[] = {FIXTURE("restated.h"), FIXTURE("restated_types.h")};
    static const char *const restated_includes[] = {"restated.h", "restated_types.h"};
    describe_names standard = describe_names_of_the_standard();
    describe_names restated_names;
    describe_names nested_names;
    static const char *const nested[] = {FIXTURE("nested.h")};
    static const char *const nested_includes[] = {"nested.h"};
    static const char *const nested_elsewhere[] = {"probe_remote"};
    golden_case cases[6];
    size_t at;
    restated_names = standard;
    restated_names.status = "probe_status";
    restated_names.refusal = "probe_refusal";
    restated_names.value = "probe_value";
    restated_names.tree = "probe_tree";
    cases[0].golden = "config.c";
    cases[0].service = service_of("weivel_standard_config", "describe_config", "config_", NULL);
    cases[1].golden = "secrets.c";
    cases[1].service = service_of("weivel_standard_secrets", "describe_secrets", "secrets_", NULL);
    cases[2].golden = "paths.c";
    cases[2].service = service_of("weivel_standard_paths", "describe_paths", "paths_", "WEIVEL_STANDARD_API");
    for (at = 0; at < 3; at++) {
        cases[at].paths = services;
        cases[at].includes = services_include;
        cases[at].header_count = 1;
        cases[at].names = standard;
    }
    cases[3].golden = "probe.c";
    cases[3].paths = probe;
    cases[3].includes = probe_includes;
    cases[3].header_count = COUNT_OF(probe);
    cases[3].names = standard;
    cases[3].service = service_of("probe_definition", "probe_description_of", "probe_", "WEIVEL_STANDARD_API");
    cases[4].golden = "restated.c";
    cases[4].paths = restated;
    cases[4].includes = restated_includes;
    cases[4].header_count = COUNT_OF(restated);
    cases[4].names = restated_names;
    cases[4].service = service_of("probe_restated", "probe_restated_description", "restated_", NULL);
    nested_names = standard;
    nested_names.elsewhere = nested_elsewhere;
    nested_names.elsewhere_count = COUNT_OF(nested_elsewhere);
    cases[5].golden = "nested.c";
    cases[5].paths = nested;
    cases[5].includes = nested_includes;
    cases[5].header_count = COUNT_OF(nested);
    cases[5].names = nested_names;
    cases[5].service = service_of("probe_lender", "nested_description_of", "nested_", NULL);
    for (at = 0; at < COUNT_OF(cases); at++) {
        writes_the_golden_source(&cases[at]);
    }
}

typedef struct refused_service {
    const char *label;
    describe_service service;
    size_t include_count;
    const char *include;
    /** A phrase the message holds. */
    const char *says;
} refused_service;

static void refuses_a_service_it_cannot_write(const describe_model *model, const refused_service *test) {
    describe_refusal refusal;
    describe_text source;
    describe_service service = test->service;
    const char *includes[2];
    describe_outcome outcome;
    includes[0] = test->include;
    includes[1] = test->include;
    service.includes = includes;
    service.include_count = test->include_count;
    outcome = describe_write(model, &service, &source, &refusal);
    if (outcome != DESCRIBE_REFUSED) {
        fprintf(stderr, "%s: expected a refusal, got outcome %d\n", test->label, (int)outcome);
        check_failures++;
        describe_text_release(&source);
        return;
    }
    CHECK(source.bytes == NULL && source.length == 0);
    CHECK(refusal.file == NULL && refusal.line == 0);
    if (refusal.message == NULL || strstr(refusal.message, test->says) == NULL) {
        fprintf(stderr, "%s: expected a message holding \"%s\", got \"%s\"\n", test->label, test->says,
                refusal.message == NULL ? "(null)" : refusal.message);
        check_failures++;
    }
    describe_refusal_release(&refusal);
}

static void refuses_what_it_cannot_write(void) {
    const char *paths[] = {WEIVEL_DESCRIBE_SERVICES_HEADER};
    describe_names names = describe_names_of_the_standard();
    describe_model model;
    describe_refusal refusal;
    refused_service cases[9];
    size_t at;
    if (describe_read(&model, paths, 1, &names, &refusal) != DESCRIBE_DONE) {
        CHECK(0);
        return;
    }
    for (at = 0; at < COUNT_OF(cases); at++) {
        cases[at].service = service_of("weivel_standard_paths", "describe_paths", "paths_", NULL);
        cases[at].include_count = 1;
        cases[at].include = "weivel/services.h";
    }
    cases[0].label = "symbol";
    cases[0].service.symbol = "describe paths";
    cases[0].says = "the symbol `describe paths` is not a C identifier";
    cases[1].label = "prefix";
    cases[1].service.prefix = "9paths_";
    cases[1].says = "the prefix `9paths_` is not a C identifier";
    cases[2].label = "export macro";
    cases[2].service.export_macro = "API(x)";
    cases[2].says = "the export macro `API(x)` is not a C identifier";
    cases[3].label = "include count";
    cases[3].include_count = 2;
    cases[3].says = "2 includes were named for 1 headers read";
    cases[4].label = "quoted include";
    cases[4].include = "weivel/\"services.h";
    cases[4].says = "the include `weivel/\"services.h` is empty or holds a quote";
    cases[5].label = "backslash include";
    cases[5].include = "weivel\\services.h";
    cases[5].says = "holds a quote, a backslash or a line end";
    cases[6].label = "empty include";
    cases[6].include = "";
    cases[6].says = "the include `` is empty";
    cases[7].label = "not a definition";
    cases[7].service.definition = "weivel_standard_secret";
    cases[7].says = "`weivel_standard_secret` is not a definition any scanned header declares";
    cases[8].label = "no such struct";
    cases[8].service.definition = "weivel_standard_missing";
    cases[8].says = "`weivel_standard_missing` is not a definition any scanned header declares";
    for (at = 0; at < COUNT_OF(cases); at++) {
        refuses_a_service_it_cannot_write(&model, &cases[at]);
    }
    describe_model_release(&model);
}

/** golden/probe.c is compiled into this test, so the golden source of every form is proven to build and to check. */
static void compiles_the_golden_probe_into_a_description_that_checks(void) {
    const weivel_standard_description *probe = probe_description_of();
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    if (weivel_standard_description_check(probe, &refusal) != WEIVEL_STATUS_OK) {
        fprintf(stderr, "golden/probe.c is refused by the check at %s: %s\n",
                refusal.path == NULL ? "(none)" : refusal.path, refusal.detail == NULL ? "(none)" : refusal.detail);
        weivel_standard_refusal_release(&refusal);
        check_failures++;
        return;
    }
    check_text(probe->definition, "probe_definition", "golden probe definition");
    CHECK(probe->type_count == 5 && probe->enum_count == 1 && probe->operation_count == 9);
    CHECK(probe->type_count == 5 && probe->types[2].member_count == 17 &&
          probe->types[2].members[9].offset == offsetof(probe_input, first) &&
          probe->types[2].first_shape == offsetof(probe_input, previous) + sizeof(weivel_standard_value));
}

/** golden/nested.c is compiled in too: its nested description is its own, and its remote one is filled on call. */
static void compiles_the_golden_nested_into_a_description_that_checks(void) {
    const weivel_standard_description *lender = nested_description_of();
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    if (weivel_standard_description_check(lender, &refusal) != WEIVEL_STATUS_OK) {
        fprintf(stderr, "golden/nested.c is refused by the check at %s: %s\n",
                refusal.path == NULL ? "(none)" : refusal.path, refusal.detail == NULL ? "(none)" : refusal.detail);
        weivel_standard_refusal_release(&refusal);
        check_failures++;
        return;
    }
    check_text(lender->definition, "probe_lender", "golden nested definition");
    CHECK(lender->blocking_count == 1 && strcmp(lender->blocking[0], "lend") == 0);
    CHECK(lender->definition_count == 2);
    if (lender->definition_count == 2) {
        check_text(lender->definitions[0]->definition, "probe_counter", "golden nested counter");
        CHECK(lender->definitions[0]->operation_count == 1 && lender->definitions[0]->definitions == NULL);
        CHECK(lender->definitions[1] == probe_remote_description());
    }
    CHECK(nested_description_of() == lender);
}

static void refuses_a_definition_reaching_itself(void) {
    const char *paths[] = {FIXTURE("refused/definition_cycle.h")};
    const char *includes[] = {"definition_cycle.h"};
    describe_names names = describe_names_of_the_standard();
    describe_model model;
    describe_refusal refusal;
    describe_text source;
    describe_service service = service_of("probe_ping", "describe_ping", "ping_", NULL);
    if (describe_read(&model, paths, 1, &names, &refusal) != DESCRIBE_DONE) {
        fprintf(stderr, "definition_cycle.h was not read: %s\n", refusal.message == NULL ? "" : refusal.message);
        describe_refusal_release(&refusal);
        check_failures++;
        return;
    }
    service.includes = includes;
    service.include_count = 1;
    CHECK(describe_write(&model, &service, &source, &refusal) == DESCRIBE_REFUSED);
    CHECK(source.bytes == NULL && source.length == 0);
    check_text(refusal.file, FIXTURE("refused/definition_cycle.h"), "the cycle's file");
    CHECK(refusal.line == 8);
    CHECK(refusal.message != NULL && strstr(refusal.message, "the definition `probe_ping` reaches itself") != NULL);
    describe_refusal_release(&refusal);
    describe_model_release(&model);
}

int main(void) {
    compiles_the_golden_probe_into_a_description_that_checks();
    compiles_the_golden_nested_into_a_description_that_checks();
    refuses_a_definition_reaching_itself();
    writes_each_golden_source();
    refuses_what_it_cannot_write();
    return check_summary("describe_write_test");
}
