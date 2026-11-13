#include "check.h"
#include "fixtures.h"
#include "weivel/layout.h"

#include <stdlib.h>

static const char *const ROOT = WEIVEL_STANDARD_FIXTURE_ROOT;

static void scenario(char *directory, const char *name) {
    fixture_make_directory(ROOT);
    fixture_join(directory, ROOT, name);
    CHECK(fixture_fresh_directory(directory));
}

static void write_file(const char *directory, const char *name, const char *text) {
    CHECK(fixture_write(directory, name, text));
}

static weivel_status discover(const char *directory, weivel_standard_discovery *discovery) {
    weivel_standard_discovery empty = WEIVEL_INIT(weivel_standard_discovery);
    *discovery = empty;
    return weivel_standard_discover(directory, NULL, discovery);
}

static const weivel_standard_discovered_plugin *plugin_named(const weivel_standard_discovery *discovery,
                                                              const char *import_name) {
    size_t at;
    for (at = 0; at < discovery->plugin_count; at++) {
        const weivel_manifest *view = weivel_standard_manifest_view(discovery->plugins[at].manifest);
        if (strcmp(view->import_name, import_name) == 0) {
            return &discovery->plugins[at];
        }
    }
    return NULL;
}

static const weivel_standard_discovery_failure *failure_for(const weivel_standard_discovery *discovery,
                                                            const char *directory, const char *name) {
    char path[FIXTURE_PATH_CAPACITY];
    size_t at;
    fixture_join(path, directory, name);
    for (at = 0; at < discovery->failure_count; at++) {
        if (strcmp(discovery->failures[at].file, path) == 0) {
            return &discovery->failures[at];
        }
    }
    fprintf(stderr, "no failure names %s\n", path);
    check_failures++;
    return NULL;
}

static void check_failure(const weivel_standard_discovery *discovery, const char *directory, const char *name,
                          uint32_t code, const char *path) {
    const weivel_standard_discovery_failure *failure = failure_for(discovery, directory, name);
    if (failure == NULL) {
        return;
    }
    if (failure->refusal.code != code) {
        fprintf(stderr, "%s: code %u, wanted %u (%s)\n", name, (unsigned)failure->refusal.code, (unsigned)code,
                failure->refusal.detail == NULL ? "" : failure->refusal.detail);
        check_failures++;
    }
    check_text(failure->refusal.path, path, name);
    CHECK(failure->refusal.detail != NULL && failure->refusal.detail[0] != '\0');
    CHECK(failure->refusal.fix != NULL && failure->refusal.fix[0] != '\0');
}

static void finds_a_valid_plugin(void) {
    char directory[FIXTURE_PATH_CAPACITY];
    char expected[FIXTURE_PATH_CAPACITY];
    weivel_standard_discovery discovery;
    const weivel_standard_discovered_plugin *plugin;
    scenario(directory, "valid");
    write_file(directory, "calendar.json", "{\"importName\": \"calendar\", \"api\": 2, \"entry\": \"calendar.bin\"}");
    write_file(directory, "calendar_identity.json", "{\"id\": \"forge:42/calendar\", \"name\": \"owner/calendar\"}");
    write_file(directory, "calendar.bin", "entry");
    CHECK(discover(directory, &discovery) == WEIVEL_STATUS_OK);
    CHECK(discovery.plugin_count == 1 && discovery.failure_count == 0);
    plugin = plugin_named(&discovery, "calendar");
    CHECK(plugin != NULL);
    if (plugin != NULL) {
        CHECK(weivel_standard_manifest_view(plugin->manifest)->api == 2);
        fixture_join(expected, directory, "calendar.json");
        check_text(plugin->manifest_path, expected, "manifest path");
        fixture_join(expected, directory, "calendar_identity.json");
        check_text(plugin->identity_path, expected, "identity path");
        check_text(plugin->identity_id, "forge:42/calendar", "identity id");
        check_text(plugin->repository_name, "owner/calendar", "repository name");
        CHECK(plugin->identity.kind == WEIVEL_STANDARD_IDENTITY_FORGE && plugin->identity.number == 42);
        fixture_join(expected, directory, "calendar.bin");
        check_text(plugin->entry_path, expected, "entry path");
        CHECK(plugin->entry_present == 1);
    }
    weivel_standard_discovery_release(&discovery);
    CHECK(discovery.plugins == NULL && discovery.plugin_count == 0 && discovery.failures == NULL);
    CHECK(discovery.size == sizeof(weivel_standard_discovery));
}

static void answers_an_entry_that_is_not_there(void) {
    char directory[FIXTURE_PATH_CAPACITY];
    char expected[FIXTURE_PATH_CAPACITY];
    weivel_standard_discovery discovery;
    const weivel_standard_discovered_plugin *plugin;
    scenario(directory, "entries");
    write_file(directory, "absent.json", "{\"importName\": \"absent\", \"api\": 2, \"entry\": \"dist/absent.bin\"}");
    write_file(directory, "none.json", "{\"importName\": \"none\", \"api\": 2}");
    CHECK(discover(directory, &discovery) == WEIVEL_STATUS_OK);
    CHECK(discovery.plugin_count == 2 && discovery.failure_count == 0);
    plugin = plugin_named(&discovery, "absent");
    if (plugin != NULL) {
        fixture_join(expected, directory, "dist/absent.bin");
        check_text(plugin->entry_path, expected, "absent entry path");
        CHECK(plugin->entry_present == 0);
    }
    plugin = plugin_named(&discovery, "none");
    CHECK(plugin != NULL && plugin->entry_path == NULL && plugin->entry_present == 0);
    weivel_standard_discovery_release(&discovery);
}

static void reports_each_bad_file_and_keeps_the_rest(void) {
    char directory[FIXTURE_PATH_CAPACITY];
    weivel_standard_discovery discovery;
    scenario(directory, "failures");
    write_file(directory, "good.json", "{\"importName\": \"good\", \"api\": 2}");
    write_file(directory, "invalid.json", "{\"importName\": \"invalid\", \"api\": \"two\"}");
    write_file(directory, "broken.json", "{\"importName\": ");
    write_file(directory, "beta.json", "{\"importName\": \"gamma\", \"api\": 2}");
    write_file(directory, "spoken.json", "{\"importName\": \"spoken\", \"api\": 2}");
    write_file(directory, "spoken_identity.json", "{\"id\": \"forge:1\", ");
    write_file(directory, "unnamed.json", "{\"importName\": \"unnamed\", \"api\": 2}");
    write_file(directory, "unnamed_identity.json", "{\"id\": \"forge:1\"}");
    write_file(directory, "wrong.json", "{\"importName\": \"wrong\", \"api\": 2}");
    write_file(directory, "wrong_identity.json", "{\"id\": \"no identity\", \"name\": \"owner/wrong\"}");
    write_file(directory, "typed.json", "{\"importName\": \"typed\", \"api\": 2}");
    write_file(directory, "typed_identity.json", "{\"id\": 7, \"name\": \"owner/typed\"}");
    write_file(directory, "blank.json", "{\"importName\": \"blank\", \"api\": 2}");
    write_file(directory, "blank_identity.json", "{\"id\": \"forge:3\", \"name\": \"\"}");
    CHECK(discover(directory, &discovery) == WEIVEL_STATUS_OK);
    CHECK(discovery.plugin_count == 1 && plugin_named(&discovery, "good") != NULL);
    CHECK(discovery.failure_count == 8);
    check_failure(&discovery, directory, "invalid.json", WEIVEL_STANDARD_REFUSAL_WRONG_TYPE, "api");
    check_failure(&discovery, directory, "broken.json", WEIVEL_STANDARD_REFUSAL_MALFORMED_JSON, "");
    check_failure(&discovery, directory, "beta.json", WEIVEL_STANDARD_REFUSAL_NAME_MISMATCH, "importName");
    check_failure(&discovery, directory, "spoken_identity.json", WEIVEL_STANDARD_REFUSAL_MALFORMED_JSON, "");
    check_failure(&discovery, directory, "unnamed_identity.json", WEIVEL_STANDARD_REFUSAL_MISSING_FIELD, "name");
    check_failure(&discovery, directory, "wrong_identity.json", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "id");
    check_failure(&discovery, directory, "typed_identity.json", WEIVEL_STANDARD_REFUSAL_WRONG_TYPE, "id");
    check_failure(&discovery, directory, "blank_identity.json", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "name");
    weivel_standard_discovery_release(&discovery);
}

static void reports_an_unreadable_file(void) {
    char directory[FIXTURE_PATH_CAPACITY];
    char locked[FIXTURE_PATH_CAPACITY];
    weivel_standard_discovery discovery;
    scenario(directory, "unreadable");
    write_file(directory, "good.json", "{\"importName\": \"good\", \"api\": 2}");
    write_file(directory, "hidden.json", "{\"importName\": \"hidden\", \"api\": 2}");
    fixture_join(locked, directory, "hidden.json");
    if (!fixture_lock(locked)) {
        printf("layout_test: skipped the unreadable file, this process reads every file\n");
        return;
    }
    CHECK(discover(directory, &discovery) == WEIVEL_STATUS_OK);
    fixture_unlock(locked);
    CHECK(discovery.plugin_count == 1 && plugin_named(&discovery, "good") != NULL);
    CHECK(discovery.failure_count == 1);
    check_failure(&discovery, directory, "hidden.json", WEIVEL_STANDARD_REFUSAL_UNREADABLE, "");
    weivel_standard_discovery_release(&discovery);
}

static void ignores_what_is_not_a_manifest(void) {
    char directory[FIXTURE_PATH_CAPACITY];
    char subdirectory[FIXTURE_PATH_CAPACITY];
    weivel_standard_discovery discovery;
    scenario(directory, "strays");
    write_file(directory, "notes.txt", "not a manifest");
    write_file(directory, "Upper.json", "{");
    write_file(directory, "package.json", "{\"type\": \"module\"}");
    write_file(directory, "orphan_identity.json", "{");
    write_file(directory, "trailing.json.bak", "{");
    write_file(directory, ".json", "{");
    write_file(directory, "only.json", "{\"importName\": \"only\", \"api\": 2}");
    fixture_join(subdirectory, directory, "folder.json");
    CHECK(fixture_make_directory(subdirectory));
    CHECK(discover(directory, &discovery) == WEIVEL_STATUS_OK);
    CHECK(discovery.plugin_count == 1 && plugin_named(&discovery, "only") != NULL);
    CHECK(discovery.failure_count == 0);
    weivel_standard_discovery_release(&discovery);
}

static void never_follows_a_link(void) {
    char directory[FIXTURE_PATH_CAPACITY];
    char target[FIXTURE_PATH_CAPACITY];
    weivel_standard_discovery discovery;
    scenario(directory, "links");
    write_file(directory, "real.json", "{\"importName\": \"real\", \"api\": 2}");
    fixture_join(target, directory, "real.json");
    if (!fixture_link(directory, "linked.json", target)) {
        printf("layout_test: skipped the link, this process may not make one\n");
        return;
    }
    CHECK(discover(directory, &discovery) == WEIVEL_STATUS_OK);
    CHECK(discovery.plugin_count == 1 && plugin_named(&discovery, "real") != NULL);
    CHECK(discovery.failure_count == 0);
    weivel_standard_discovery_release(&discovery);
}

static void never_reaches_an_entry_through_a_link(void) {
    char directory[FIXTURE_PATH_CAPACITY];
    char outside[FIXTURE_PATH_CAPACITY];
    char real[FIXTURE_PATH_CAPACITY];
    weivel_standard_discovery discovery;
    const weivel_standard_discovered_plugin *plugin;
    scenario(outside, "outside");
    write_file(outside, "main.bin", "entry");
    scenario(directory, "linked-entry");
    write_file(directory, "linked.json", "{\"importName\": \"linked\", \"api\": 2, \"entry\": \"lib/main.bin\"}");
    write_file(directory, "real.json", "{\"importName\": \"real\", \"api\": 2, \"entry\": \"real/main.bin\"}");
    fixture_join(real, directory, "real");
    CHECK(fixture_make_directory(real));
    write_file(real, "main.bin", "entry");
    if (!fixture_link_directory(directory, "lib", outside)) {
        printf("layout_test: skipped the linked entry, this process may not make a link\n");
        return;
    }
    CHECK(discover(directory, &discovery) == WEIVEL_STATUS_OK);
    CHECK(discovery.plugin_count == 2 && discovery.failure_count == 0);
    plugin = plugin_named(&discovery, "linked");
    CHECK(plugin != NULL && plugin->entry_path != NULL && plugin->entry_present == 0);
    plugin = plugin_named(&discovery, "real");
    CHECK(plugin != NULL && plugin->entry_present == 1);
    weivel_standard_discovery_release(&discovery);
}

/** A manifest of exactly `size` bytes: `text`, then spaces. */
static void write_sized(const char *directory, const char *name, const char *text, size_t size) {
    char *padded = malloc(size + 1);
    CHECK(padded != NULL);
    if (padded == NULL) {
        return;
    }
    memset(padded, ' ', size);
    memcpy(padded, text, strlen(text));
    padded[size] = '\0';
    write_file(directory, name, padded);
    free(padded);
}

static void refuses_a_file_over_the_cap_alone(void) {
    static const size_t CAP = (size_t)1 << 20;
    char directory[FIXTURE_PATH_CAPACITY];
    weivel_standard_discovery discovery;
    scenario(directory, "sizes");
    write_sized(directory, "exact.json", "{\"importName\": \"exact\", \"api\": 2}", CAP);
    write_sized(directory, "big.json", "{\"importName\": \"big\", \"api\": 2}", CAP + 1);
    write_file(directory, "huge.json", "{\"importName\": \"huge\", \"api\": 2}");
    write_sized(directory, "huge_identity.json", "{\"id\": \"forge:5\", \"name\": \"o/huge\"}", CAP + 1);
    write_file(directory, "good.json", "{\"importName\": \"good\", \"api\": 2}");
    CHECK(discover(directory, &discovery) == WEIVEL_STATUS_OK);
    CHECK(discovery.plugin_count == 2 && plugin_named(&discovery, "exact") != NULL &&
          plugin_named(&discovery, "good") != NULL);
    CHECK(discovery.failure_count == 2);
    check_failure(&discovery, directory, "big.json", WEIVEL_STANDARD_REFUSAL_UNREADABLE, "");
    check_failure(&discovery, directory, "huge_identity.json", WEIVEL_STANDARD_REFUSAL_UNREADABLE, "");
    weivel_standard_discovery_release(&discovery);
}

static const char *import_name_at(const weivel_standard_discovery *discovery, size_t at) {
    return at < discovery->plugin_count ? weivel_standard_manifest_view(discovery->plugins[at].manifest)->import_name
                                        : "(none)";
}

static void orders_by_identity_then_the_rest_by_name(void) {
    static const char *const ORDER[] = {"zulu", "yankee", "xray", "whiskey", "able", "baker"};
    char directory[FIXTURE_PATH_CAPACITY];
    weivel_standard_discovery discovery;
    size_t at;
    scenario(directory, "order");
    write_file(directory, "baker.json", "{\"importName\": \"baker\", \"api\": 2}");
    write_file(directory, "able.json", "{\"importName\": \"able\", \"api\": 2}");
    write_file(directory, "whiskey.json", "{\"importName\": \"whiskey\", \"api\": 2}");
    write_file(directory, "whiskey_identity.json", "{\"id\": \"forge@host.example:9\", \"name\": \"o/w\"}");
    write_file(directory, "xray.json", "{\"importName\": \"xray\", \"api\": 2}");
    write_file(directory, "xray_identity.json", "{\"id\": \"forge:10\", \"name\": \"o/x\"}");
    write_file(directory, "yankee.json", "{\"importName\": \"yankee\", \"api\": 2}");
    write_file(directory, "yankee_identity.json", "{\"id\": \"forge:9\", \"name\": \"o/y\"}");
    write_file(directory, "zulu.json", "{\"importName\": \"zulu\", \"api\": 2}");
    write_file(directory, "zulu_identity.json", "{\"id\": \"local:zulu\", \"name\": \"zulu\"}");
    CHECK(discover(directory, &discovery) == WEIVEL_STATUS_OK);
    CHECK(discovery.plugin_count == 6 && discovery.failure_count == 0);
    for (at = 0; at < sizeof ORDER / sizeof ORDER[0]; at++) {
        check_text(import_name_at(&discovery, at), ORDER[at], "plugin order");
    }
    CHECK(discovery.plugin_count < 5 || discovery.plugins[4].identity_path == NULL);
    weivel_standard_discovery_release(&discovery);
}

static void refuses_a_repeated_identity(void) {
    char directory[FIXTURE_PATH_CAPACITY];
    weivel_standard_discovery discovery;
    scenario(directory, "repeated");
    write_file(directory, "first.json", "{\"importName\": \"first\", \"api\": 2}");
    write_file(directory, "first_identity.json", "{\"id\": \"forge:7\", \"name\": \"o/r\"}");
    write_file(directory, "second.json", "{\"importName\": \"second\", \"api\": 2}");
    write_file(directory, "second_identity.json", "{\"id\": \"forge:007\", \"name\": \"o/r\"}");
    CHECK(discover(directory, &discovery) == WEIVEL_STATUS_OK);
    CHECK(discovery.plugin_count == 1 && plugin_named(&discovery, "first") != NULL);
    CHECK(discovery.failure_count == 1);
    check_failure(&discovery, directory, "second_identity.json", WEIVEL_STANDARD_REFUSAL_DUPLICATE, "id");
    weivel_standard_discovery_release(&discovery);
}

static void reads_a_directory_whose_name_is_not_ascii(void) {
    char directory[FIXTURE_PATH_CAPACITY];
    char expected[FIXTURE_PATH_CAPACITY];
    weivel_standard_discovery discovery;
    scenario(directory, "plugins-\xc3\xbc-\xe6\x97\xa5");
    write_file(directory, "wide.json", "{\"importName\": \"wide\", \"api\": 2, \"entry\": \"wide.bin\"}");
    write_file(directory, "wide.bin", "entry");
    CHECK(discover(directory, &discovery) == WEIVEL_STATUS_OK);
    CHECK(discovery.plugin_count == 1 && discovery.failure_count == 0);
    if (discovery.plugin_count == 1) {
        fixture_join(expected, directory, "wide.json");
        check_text(discovery.plugins[0].manifest_path, expected, "non-ascii manifest path");
        CHECK(discovery.plugins[0].entry_present == 1);
    }
    weivel_standard_discovery_release(&discovery);
}

static void answers_nothing_found_for_an_empty_directory(void) {
    char directory[FIXTURE_PATH_CAPACITY];
    weivel_standard_discovery discovery;
    scenario(directory, "empty");
    CHECK(discover(directory, &discovery) == WEIVEL_STATUS_OK);
    CHECK(discovery.plugin_count == 0 && discovery.failure_count == 0);
    CHECK(discovery.refusal.code == WEIVEL_STANDARD_REFUSAL_NONE);
    weivel_standard_discovery_release(&discovery);
}

static void refuses_a_directory_that_is_not_there(void) {
    char directory[FIXTURE_PATH_CAPACITY];
    weivel_standard_discovery discovery;
    fixture_make_directory(ROOT);
    fixture_join(directory, ROOT, "missing");
    fixture_remove_directory(directory);
    CHECK(discover(directory, &discovery) == WEIVEL_STATUS_REFUSED);
    CHECK(discovery.refusal.code == WEIVEL_STANDARD_REFUSAL_NOT_FOUND);
    CHECK(discovery.refusal.detail != NULL && strstr(discovery.refusal.detail, "missing") != NULL);
    CHECK(discovery.plugin_count == 0 && discovery.failure_count == 0);
    weivel_standard_discovery_release(&discovery);
    CHECK(discovery.refusal.storage == NULL);
}

static void refuses_a_file_named_as_the_directory(void) {
    char directory[FIXTURE_PATH_CAPACITY];
    char file[FIXTURE_PATH_CAPACITY];
    weivel_standard_discovery discovery;
    scenario(directory, "not-a-directory");
    write_file(directory, "plain.json", "{\"importName\": \"plain\", \"api\": 2}");
    fixture_join(file, directory, "plain.json");
    CHECK(discover(file, &discovery) == WEIVEL_STATUS_REFUSED);
    CHECK(discovery.refusal.code == WEIVEL_STANDARD_REFUSAL_UNREADABLE);
    weivel_standard_discovery_release(&discovery);
}

static void refuses_a_malformed_call(void) {
    char directory[FIXTURE_PATH_CAPACITY];
    weivel_standard_discovery discovery;
    weivel_standard_discovery small = {0};
    scenario(directory, "calls");
    write_file(directory, "held.json", "{\"importName\": \"held\", \"api\": 2}");
    CHECK(discover(NULL, &discovery) == WEIVEL_STATUS_MALFORMED);
    CHECK(discovery.refusal.code == WEIVEL_STANDARD_REFUSAL_MALFORMED_CALL);
    weivel_standard_discovery_release(&discovery);
    CHECK(discover("", &discovery) == WEIVEL_STATUS_MALFORMED);
    weivel_standard_discovery_release(&discovery);
    CHECK(weivel_standard_discover(directory, NULL, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(weivel_standard_discover(directory, NULL, &small) == WEIVEL_STATUS_MALFORMED);
    CHECK(small.refusal.storage == NULL);
    CHECK(discover(directory, &discovery) == WEIVEL_STATUS_OK);
    CHECK(weivel_standard_discover(directory, NULL, &discovery) == WEIVEL_STATUS_MALFORMED);
    CHECK(discovery.plugin_count == 1 && discovery.refusal.storage == NULL);
    weivel_standard_discovery_release(&discovery);
    weivel_standard_discovery_release(NULL);
}

static void passes_the_well_known_services_on(void) {
    static const char *const WELL_KNOWN[] = {"accounts"};
    weivel_strings well_known = {WELL_KNOWN, 1};
    weivel_standard_validation_options options = {0};
    char directory[FIXTURE_PATH_CAPACITY];
    weivel_standard_discovery discovery = WEIVEL_INIT(weivel_standard_discovery);
    options.size = (uint32_t)sizeof options;
    options.well_known_services = &well_known;
    scenario(directory, "options");
    write_file(directory, "bare.json",
               "{\"importName\": \"bare\", \"api\": 2, \"services\": {\"provides\": [\"calendar\"]}}");
    CHECK(weivel_standard_discover(directory, &options, &discovery) == WEIVEL_STATUS_OK);
    CHECK(discovery.plugin_count == 0 && discovery.failure_count == 1);
    check_failure(&discovery, directory, "bare.json", WEIVEL_STANDARD_REFUSAL_NOT_WELL_KNOWN, "services.provides[0]");
    weivel_standard_discovery_release(&discovery);
}

int main(void) {
    finds_a_valid_plugin();
    answers_an_entry_that_is_not_there();
    reports_each_bad_file_and_keeps_the_rest();
    reports_an_unreadable_file();
    ignores_what_is_not_a_manifest();
    never_follows_a_link();
    never_reaches_an_entry_through_a_link();
    refuses_a_file_over_the_cap_alone();
    orders_by_identity_then_the_rest_by_name();
    refuses_a_repeated_identity();
    reads_a_directory_whose_name_is_not_ascii();
    answers_nothing_found_for_an_empty_directory();
    refuses_a_directory_that_is_not_there();
    refuses_a_file_named_as_the_directory();
    refuses_a_malformed_call();
    passes_the_well_known_services_on();
    return check_summary("layout_test");
}
