#include "check.h"
#include "weivel/standard.h"

#include <stdlib.h>

/**
 * Reads from a heap copy that is scribbled over and freed before the answer is looked at, so every test also proves
 * the manifest keeps nothing of the caller's text.
 */
static weivel_status read_text(const char *text, weivel_standard_manifest **manifest,
                               weivel_standard_refusal *refusal) {
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    weivel_status status;
    if (copy == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    memcpy(copy, text, length + 1);
    status = weivel_standard_read_manifest(copy, length, manifest, refusal);
    memset(copy, 'x', length);
    free(copy);
    return status;
}

static const weivel_manifest *read_view(const char *text, weivel_standard_manifest **manifest) {
    weivel_standard_refusal refusal = {0};
    weivel_status status = read_text(text, manifest, &refusal);
    if (status != WEIVEL_STATUS_OK) {
        fprintf(stderr, "reading %s answered %d: %s (at \"%s\")\n", text, (int)status,
                refusal.detail == NULL ? "(none)" : refusal.detail, refusal.path == NULL ? "" : refusal.path);
    }
    CHECK(status == WEIVEL_STATUS_OK);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_NONE && refusal.storage == NULL);
    weivel_standard_refusal_release(&refusal);
    return weivel_standard_manifest_view(*manifest);
}

static void check_strings(weivel_strings strings, const char *const *expected, uint32_t count, const char *label) {
    uint32_t at;
    CHECK(strings.count == count);
    for (at = 0; at < count && at < strings.count; at++) {
        check_text(strings.items[at], expected[at], label);
    }
}

static const char *const FULL_MANIFEST =
    "{\"importName\": \"greeter\", \"api\": 2, \"entry\": \"dist/greeter.so\","
    " \"targets\": [\"app.one\", \"app.two\"],"
    " \"capabilities\": {\"provides\": [\"screens\", \"menus\"], \"consumes\": [\"provider\"]},"
    " \"services\": {"
    "   \"provides\": [\"greeter:hello\", {\"id\": \"greeter:fast\", \"attributes\": {\"latency\": 5, \"throughput\": 1.5}}],"
    "   \"consumes\": [\"clock\", {\"id\": \"store\", \"optional\": false, \"provider\": \"local:store\","
    "                  \"degradable\": true, \"constraints\": {\"latency\": 10}}]},"
    " \"permissions\": [\"network\", \"files\"],"
    " \"dependencies\": [{\"importName\": \"store\", \"id\": \"local:store\"}]}";

static void check_full_offers(weivel_service_offers offers) {
    CHECK(offers.count == 2);
    if (offers.count != 2) {
        return;
    }
    CHECK(offers.items[0].size == sizeof(weivel_service_offer));
    check_text(offers.items[0].id, "greeter:hello", "services.provides[0]");
    CHECK(offers.items[0].attributes.count == 0 && offers.items[0].attributes.items == NULL);
    CHECK(offers.items[1].size == sizeof(weivel_service_offer));
    check_text(offers.items[1].id, "greeter:fast", "services.provides[1].id");
    CHECK(offers.items[1].attributes.count == 2);
    if (offers.items[1].attributes.count == 2) {
        CHECK(offers.items[1].attributes.items[0].size == sizeof(weivel_number));
        check_text(offers.items[1].attributes.items[0].name, "latency", "attribute 0");
        CHECK(offers.items[1].attributes.items[0].value == 5.0);
        check_text(offers.items[1].attributes.items[1].name, "throughput", "attribute 1");
        CHECK(offers.items[1].attributes.items[1].value == 1.5);
    }
}

static void check_full_wants(weivel_service_wants wants) {
    CHECK(wants.count == 2);
    if (wants.count != 2) {
        return;
    }
    CHECK(wants.items[0].size == sizeof(weivel_service_want));
    check_text(wants.items[0].id, "clock", "services.consumes[0]");
    CHECK(wants.items[0].provider == NULL);
    CHECK(wants.items[0].optional == WEIVEL_CHOICE_DEFAULT);
    CHECK(wants.items[0].degradable == WEIVEL_CHOICE_DEFAULT);
    CHECK(wants.items[0].constraints.count == 0);
    CHECK(wants.items[1].size == sizeof(weivel_service_want));
    check_text(wants.items[1].id, "store", "services.consumes[1].id");
    check_text(wants.items[1].provider, "local:store", "services.consumes[1].provider");
    CHECK(wants.items[1].optional == WEIVEL_CHOICE_NO);
    CHECK(wants.items[1].degradable == WEIVEL_CHOICE_YES);
    CHECK(wants.items[1].constraints.count == 1);
    if (wants.items[1].constraints.count == 1) {
        CHECK(wants.items[1].constraints.items[0].size == sizeof(weivel_number));
        check_text(wants.items[1].constraints.items[0].name, "latency", "constraint 0");
        CHECK(wants.items[1].constraints.items[0].value == 10.0);
    }
}

static void every_core_subset_field_lands(void) {
    static const char *const targets[] = {"app.one", "app.two"};
    static const char *const provided[] = {"screens", "menus"};
    static const char *const consumed[] = {"provider"};
    static const char *const permissions[] = {"network", "files"};
    weivel_standard_manifest *manifest = NULL;
    const weivel_manifest *view = read_view(FULL_MANIFEST, &manifest);
    CHECK(view != NULL);
    if (view == NULL) {
        return;
    }
    CHECK(view->size == sizeof(weivel_manifest));
    check_text(view->import_name, "greeter", "importName");
    CHECK(view->api == 2);
    check_text(view->entry, "dist/greeter.so", "entry");
    check_strings(view->targets, targets, 2, "targets");
    check_strings(view->capabilities.provides, provided, 2, "capabilities.provides");
    check_strings(view->capabilities.consumes, consumed, 1, "capabilities.consumes");
    check_full_offers(view->services.provides);
    check_full_wants(view->services.consumes);
    check_strings(view->permissions, permissions, 2, "permissions");
    CHECK(view->identity == NULL);
    weivel_standard_manifest_release(manifest);
}

static void absent_fields_take_the_core_defaults(void) {
    weivel_standard_manifest *manifest = NULL;
    const weivel_manifest *view = read_view("{\"importName\": \"bare\"}", &manifest);
    CHECK(view != NULL);
    if (view != NULL) {
        check_text(view->import_name, "bare", "importName");
        CHECK(view->api == 0);
        CHECK(view->entry == NULL);
        CHECK(view->targets.count == 0 && view->targets.items == NULL);
        CHECK(view->capabilities.provides.count == 0 && view->capabilities.consumes.count == 0);
        CHECK(view->services.provides.count == 0 && view->services.provides.items == NULL);
        CHECK(view->services.consumes.count == 0 && view->services.consumes.items == NULL);
        CHECK(view->permissions.count == 0 && view->permissions.items == NULL);
        CHECK(view->identity == NULL);
    }
    weivel_standard_manifest_release(manifest);
    manifest = NULL;
    view = read_view("{}", &manifest);
    CHECK(view != NULL && view->import_name == NULL);
    weivel_standard_manifest_release(manifest);
}

static void a_consumed_service_reads_in_both_forms(void) {
    weivel_standard_manifest *manifest = NULL;
    const weivel_manifest *view = read_view(
        "{\"importName\": \"forms\", \"services\": {\"consumes\": [\"bare\", {\"id\": \"object\"},"
        " {\"id\": \"wanted\", \"optional\": true, \"degradable\": false}]}}",
        &manifest);
    weivel_service_wants wants;
    CHECK(view != NULL);
    if (view == NULL) {
        return;
    }
    wants = view->services.consumes;
    CHECK(wants.count == 3);
    if (wants.count == 3) {
        check_text(wants.items[0].id, "bare", "string form");
        check_text(wants.items[1].id, "object", "object form");
        CHECK(wants.items[0].optional == wants.items[1].optional);
        CHECK(wants.items[0].degradable == wants.items[1].degradable);
        CHECK(wants.items[0].provider == NULL && wants.items[1].provider == NULL);
        CHECK(wants.items[1].optional == WEIVEL_CHOICE_DEFAULT && wants.items[1].constraints.count == 0);
        check_text(wants.items[2].id, "wanted", "object form with choices");
        CHECK(wants.items[2].optional == WEIVEL_CHOICE_YES);
        CHECK(wants.items[2].degradable == WEIVEL_CHOICE_NO);
    }
    weivel_standard_manifest_release(manifest);
}

static void unknown_fields_are_ignored(void) {
    weivel_standard_manifest *manifest = NULL;
    const weivel_manifest *view = read_view(
        "{\"importName\": \"open\", \"ui\": {\"screens\": [1, 2]}, \"settings\": null, \"dependencies\": 5,"
        " \"capabilities\": {\"provides\": [\"a\"], \"later\": true},"
        " \"services\": {\"other\": null, \"provides\": [{\"id\": \"open:x\", \"weight\": \"heavy\"}],"
        "   \"consumes\": [{\"id\": \"y\", \"note\": [null]}]}}",
        &manifest);
    CHECK(view != NULL);
    if (view != NULL) {
        check_text(view->import_name, "open", "importName");
        CHECK(view->capabilities.provides.count == 1);
        CHECK(view->services.provides.count == 1 && view->services.provides.items[0].attributes.count == 0);
        CHECK(view->services.consumes.count == 1);
        if (view->services.consumes.count == 1) {
            check_text(view->services.consumes.items[0].id, "y", "services.consumes[0].id");
        }
    }
    weivel_standard_manifest_release(manifest);
}

static void check_refused_at(const char *text, uint32_t code, const char *path) {
    weivel_standard_manifest *manifest = (weivel_standard_manifest *)&manifest;
    weivel_standard_refusal refusal = {0};
    weivel_status status = read_text(text, &manifest, &refusal);
    if (status != WEIVEL_STATUS_REFUSED) {
        fprintf(stderr, "reading %s answered %d, not refused\n", text, (int)status);
    }
    CHECK(status == WEIVEL_STATUS_REFUSED);
    CHECK(manifest == NULL);
    if (refusal.code != code) {
        fprintf(stderr, "reading %s: expected code %u, got %u\n", text, (unsigned)code, (unsigned)refusal.code);
    }
    CHECK(refusal.code == code);
    check_text(refusal.path, path, text);
    CHECK(refusal.detail != NULL && refusal.detail[0] != '\0');
    CHECK(refusal.fix != NULL && refusal.fix[0] != '\0');
    weivel_standard_refusal_release(&refusal);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_NONE && refusal.storage == NULL && refusal.path == NULL);
}

static void check_wrong_type_at(const char *text, const char *path) {
    check_refused_at(text, WEIVEL_STANDARD_REFUSAL_WRONG_TYPE, path);
}

static void a_wrong_type_is_refused_at_its_path(void) {
    check_wrong_type_at("[]", "");
    check_wrong_type_at("\"greeter\"", "");
    check_wrong_type_at("{\"importName\": 3}", "importName");
    check_wrong_type_at("{\"importName\": null}", "importName");
    check_wrong_type_at("{\"api\": \"2\"}", "api");
    check_wrong_type_at("{\"api\": 1.5}", "api");
    check_wrong_type_at("{\"entry\": true}", "entry");
    check_wrong_type_at("{\"targets\": \"app\"}", "targets");
    check_wrong_type_at("{\"targets\": [\"a\", 1]}", "targets[1]");
    check_wrong_type_at("{\"capabilities\": [\"x\"]}", "capabilities");
    check_wrong_type_at("{\"capabilities\": {\"provides\": [\"a\", \"b\", null]}}", "capabilities.provides[2]");
    check_wrong_type_at("{\"capabilities\": {\"consumes\": {}}}", "capabilities.consumes");
    check_wrong_type_at("{\"services\": []}", "services");
    check_wrong_type_at("{\"services\": {\"provides\": [1]}}", "services.provides[0]");
    check_wrong_type_at("{\"services\": {\"provides\": [{\"id\": 1}]}}", "services.provides[0].id");
    check_wrong_type_at("{\"services\": {\"provides\": [\"a\", {\"id\": \"b\", \"attributes\": [1]}]}}",
                        "services.provides[1].attributes");
    check_wrong_type_at("{\"services\": {\"provides\": [{\"id\": \"a\", \"attributes\": {\"latency\": \"fast\"}}]}}",
                        "services.provides[0].attributes.latency");
    check_wrong_type_at("{\"services\": {\"consumes\": \"a\"}}", "services.consumes");
    check_wrong_type_at("{\"services\": {\"consumes\": [false]}}", "services.consumes[0]");
    check_wrong_type_at("{\"services\": {\"consumes\": [\"a\", {\"id\": \"b\", \"provider\": 7}]}}",
                        "services.consumes[1].provider");
    check_wrong_type_at("{\"services\": {\"consumes\": [{\"id\": \"a\", \"optional\": \"yes\"}]}}",
                        "services.consumes[0].optional");
    check_wrong_type_at("{\"services\": {\"consumes\": [{\"id\": \"a\", \"degradable\": 1}]}}",
                        "services.consumes[0].degradable");
    check_wrong_type_at("{\"services\": {\"consumes\": [{\"id\": \"a\", \"constraints\": {\"latency\": true}}]}}",
                        "services.consumes[0].constraints.latency");
    check_wrong_type_at("{\"services\": {\"consumes\": [{\"id\": \"a\", \"constraints\": 3}]}}",
                        "services.consumes[0].constraints");
    check_wrong_type_at("{\"permissions\": [null]}", "permissions[0]");
}

static void an_unrepresentable_value_is_refused_at_its_path(void) {
    check_refused_at("{\"api\": -1}", WEIVEL_STANDARD_REFUSAL_UNREPRESENTABLE, "api");
    check_refused_at("{\"api\": 4294967296}", WEIVEL_STANDARD_REFUSAL_UNREPRESENTABLE, "api");
    check_refused_at("{\"entry\": \"a\\u0000b\"}", WEIVEL_STANDARD_REFUSAL_UNREPRESENTABLE, "entry");
    check_refused_at("{\"services\": {\"consumes\": [{\"id\": \"a\\u0000\"}]}}", WEIVEL_STANDARD_REFUSAL_UNREPRESENTABLE,
                     "services.consumes[0].id");
}

static void a_whole_api_number_is_read_in_any_spelling(void) {
    weivel_standard_manifest *manifest = NULL;
    const weivel_manifest *view = read_view("{\"api\": 3.0}", &manifest);
    CHECK(view != NULL && view->api == 3);
    weivel_standard_manifest_release(manifest);
    manifest = NULL;
    view = read_view("{\"api\": 4294967295}", &manifest);
    CHECK(view != NULL && view->api == 4294967295u);
    weivel_standard_manifest_release(manifest);
}

static void a_refusal_says_what_and_how_to_fix_it(void) {
    weivel_standard_manifest *manifest = NULL;
    weivel_standard_refusal refusal = {0};
    CHECK(read_text("{\"services\": {\"consumes\": [\"a\", {\"id\": \"b\", \"provider\": 7}]}}", &manifest,
                    &refusal) == WEIVEL_STATUS_REFUSED);
    check_text(refusal.detail, "services.consumes[1].provider is a number, where a string is needed", "detail");
    check_text(refusal.fix, "write services.consumes[1].provider as a string", "fix");
    weivel_standard_refusal_release(&refusal);
    CHECK(read_text("[]", &manifest, &refusal) == WEIVEL_STATUS_REFUSED);
    check_text(refusal.detail, "the manifest is an array, where an object is needed", "root detail");
    check_text(refusal.fix, "write the manifest as an object", "root fix");
    weivel_standard_refusal_release(&refusal);
}

static void malformed_json_is_refused_with_its_place(void) {
    weivel_standard_manifest *manifest = NULL;
    weivel_standard_refusal refusal = {0};
    CHECK(read_text("{\"importName\": }", &manifest, &refusal) == WEIVEL_STATUS_REFUSED);
    CHECK(manifest == NULL);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_MALFORMED_JSON);
    check_text(refusal.path, "", "malformed json path");
    CHECK(refusal.detail != NULL && strncmp(refusal.detail, "line 1, column 16: ", 19) == 0);
    CHECK(refusal.fix != NULL && strstr(refusal.fix, "line 1, column 16") != NULL);
    weivel_standard_refusal_release(&refusal);
    CHECK(weivel_standard_read_manifest("{}", 0, &manifest, &refusal) == WEIVEL_STATUS_REFUSED);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_MALFORMED_JSON);
    weivel_standard_refusal_release(&refusal);
}

static void the_length_bounds_the_text(void) {
    weivel_standard_manifest *manifest = NULL;
    const char *text = "{\"importName\": \"cut\"}trailing";
    CHECK(weivel_standard_read_manifest(text, 21, &manifest, NULL) == WEIVEL_STATUS_OK);
    CHECK(manifest != NULL);
    if (manifest != NULL) {
        check_text(weivel_standard_manifest_view(manifest)->import_name, "cut", "bounded text");
    }
    weivel_standard_manifest_release(manifest);
}

static void a_malformed_call_is_answered_not_crashed(void) {
    weivel_standard_manifest *manifest = NULL;
    weivel_standard_refusal refusal = {0};
    weivel_standard_refusal held = {0};
    CHECK(weivel_standard_read_manifest(NULL, 2, &manifest, &refusal) == WEIVEL_STATUS_MALFORMED);
    CHECK(manifest == NULL);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_MALFORMED_CALL && refusal.detail != NULL && refusal.fix != NULL);
    check_text(refusal.path, "", "malformed call path");
    weivel_standard_refusal_release(&refusal);
    CHECK(weivel_standard_read_manifest("{}", 2, NULL, &refusal) == WEIVEL_STATUS_MALFORMED);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_MALFORMED_CALL);
    CHECK(read_text("[]", &manifest, &held) == WEIVEL_STATUS_REFUSED);
    CHECK(weivel_standard_read_manifest("{}", 2, &manifest, &held) == WEIVEL_STATUS_MALFORMED);
    CHECK(held.code == WEIVEL_STANDARD_REFUSAL_WRONG_TYPE);
    check_text(held.path, "", "a held refusal is left as it was");
    weivel_standard_refusal_release(&held);
    weivel_standard_refusal_release(&refusal);
    weivel_standard_refusal_release(NULL);
    weivel_standard_manifest_release(NULL);
    CHECK(weivel_standard_manifest_view(NULL) == NULL);
}

static weivel_handle register_text(weivel_core *core, const char *text) {
    weivel_standard_manifest *manifest = NULL;
    weivel_handle plugin = 0;
    const weivel_manifest *view = read_view(text, &manifest);
    CHECK(view != NULL);
    if (view != NULL) {
        CHECK(weivel_register_manifest(core, view, &plugin, NULL) == WEIVEL_STATUS_OK);
    }
    weivel_standard_manifest_release(manifest);
    return plugin;
}

static uint32_t support_axis(weivel_core *core, const char *text) {
    weivel_standard_manifest *manifest = NULL;
    weivel_support answer = WEIVEL_INIT(weivel_support);
    const weivel_manifest *view = read_view(text, &manifest);
    uint32_t axis = 99;
    if (view != NULL && weivel_supports(core, view, &answer, NULL) == WEIVEL_STATUS_OK) {
        axis = answer.supported ? WEIVEL_AXIS_NONE : answer.axis;
        CHECK(answer.supported || answer.refusal.code == WEIVEL_REFUSAL_INCOMPATIBLE);
        weivel_support_release(&answer);
    }
    weivel_standard_manifest_release(manifest);
    return axis;
}

static weivel_core *create_core(void) {
    static const char *const applications[] = {"app.one"};
    weivel_core_options options = WEIVEL_INIT(weivel_core_options);
    weivel_core *core = NULL;
    options.applications.items = applications;
    options.applications.count = 1;
    CHECK(weivel_core_create(&options, &core, NULL) == WEIVEL_STATUS_OK);
    return core;
}

static void the_core_answers_what_a_manifest_supports(weivel_core *core) {
    CHECK(support_axis(core, "{\"importName\": \"fits\", \"api\": 2, \"targets\": [\"app.one\"]}") == WEIVEL_AXIS_NONE);
    CHECK(support_axis(core, "{\"importName\": \"elsewhere\", \"api\": 1, \"targets\": [\"app.two\"]}") ==
          WEIVEL_AXIS_APPLICATION);
    CHECK(support_axis(core, "{\"importName\": \"ahead\", \"api\": 3}") == WEIVEL_AXIS_API);
}

static int plan_has_edge(const weivel_activation_plan *plan, weivel_handle provider, weivel_handle consumer) {
    uint32_t at;
    for (at = 0; at < plan->edge_count; at++) {
        if (plan->edges[at].provider == provider && plan->edges[at].consumer == consumer) {
            return 1;
        }
    }
    return 0;
}

static void the_core_plans_what_the_manifests_declare(weivel_core *core) {
    weivel_handle plugins[4];
    weivel_activation_plan plan = WEIVEL_INIT(weivel_activation_plan);
    plugins[0] = register_text(core, "{\"importName\": \"reader\", \"api\": 1, \"capabilities\": {\"provides\": [\"screens\"]},"
                                     " \"permissions\": [\"network\"],"
                                     " \"services\": {\"consumes\": [{\"id\": \"store:data\", \"optional\": false}]}}");
    plugins[1] = register_text(core, "{\"importName\": \"store\", \"api\": 1, \"services\": {\"provides\": [\"store:data\"]}}");
    plugins[2] = register_text(core, "{\"importName\": \"needy\", \"services\": {\"consumes\": [{\"id\": \"gone:thing\","
                                     " \"optional\": false}]}}");
    plugins[3] = register_text(core, "{\"importName\": \"lax\", \"services\": {\"consumes\": [\"gone:thing\"]}}");
    CHECK(weivel_plan_activation(core, plugins, 4, WEIVEL_EDGES_SERVICES, NULL, &plan, NULL) == WEIVEL_STATUS_OK);
    CHECK(plan_has_edge(&plan, plugins[1], plugins[0]));
    CHECK(plan.edge_count == 1);
    CHECK(plan.refusal_count == 1);
    if (plan.refusal_count == 1) {
        CHECK(plan.refusals[0].plugin == plugins[2]);
        CHECK(plan.refusals[0].refusal.code == WEIVEL_REFUSAL_UNMET_NEED);
    }
    CHECK(plan.order_count == 3);
    if (plan.order_count == 3) {
        CHECK(plan.order[0] == plugins[1] && plan.order[1] == plugins[3] && plan.order[2] == plugins[0]);
    }
    weivel_activation_plan_release(&plan);
}

static void the_core_keeps_what_was_declared(weivel_core *core) {
    weivel_handle plugin = register_text(core, "{\"importName\": \"kept\", \"capabilities\": {\"provides\": [\"menus\", \"tabs\"]},"
                                               " \"permissions\": [\"clock\"]}");
    weivel_ledger_row row = WEIVEL_INIT(weivel_ledger_row);
    static const char *const declared[] = {"menus", "tabs"};
    static const char *const permissions[] = {"clock"};
    CHECK(weivel_ledger_entry(core, plugin, &row, NULL) == WEIVEL_STATUS_OK);
    check_text(row.import_name, "kept", "ledger import name");
    check_strings(row.capabilities_declared, declared, 2, "ledger capabilities");
    check_strings(row.permissions, permissions, 1, "ledger permissions");
    weivel_ledger_row_release(&row);
}

static void the_result_registers_with_the_core(void) {
    weivel_core *core = create_core();
    weivel_event event = WEIVEL_INIT(weivel_event);
    if (core == NULL) {
        return;
    }
    the_core_answers_what_a_manifest_supports(core);
    the_core_plans_what_the_manifests_declare(core);
    the_core_keeps_what_was_declared(core);
    while (weivel_next_event(core, &event, NULL) == WEIVEL_STATUS_OK) {
        if (event.kind == WEIVEL_EVENT_REFUSAL) {
            fprintf(stderr, "the core refused: %s\n", event.data.refusal.detail);
            check_failures++;
        }
        weivel_event_release(&event);
    }
    weivel_core_destroy(core);
}

int main(void) {
    every_core_subset_field_lands();
    absent_fields_take_the_core_defaults();
    a_consumed_service_reads_in_both_forms();
    unknown_fields_are_ignored();
    a_wrong_type_is_refused_at_its_path();
    an_unrepresentable_value_is_refused_at_its_path();
    a_whole_api_number_is_read_in_any_spelling();
    a_refusal_says_what_and_how_to_fix_it();
    malformed_json_is_refused_with_its_place();
    the_length_bounds_the_text();
    a_malformed_call_is_answered_not_crashed();
    the_result_registers_with_the_core();
    return check_summary("manifest_test");
}
