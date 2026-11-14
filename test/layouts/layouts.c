#include <stdio.h>

#include "weivel/identity.h"
#include "weivel/layout.h"
#include "weivel/library.h"
#include "weivel/mirrors.h"
#include "weivel/services.h"
#include "weivel/standard.h"

/**
 * @file
 * Prints the layout of every public struct the standard's headers declare, as this compiler lays it out, so a language
 * that declares the same layouts by hand compares its own against the C compiler's rather than against a guess.
 *
 * Every line is plain text, fields separated by one space, numbers in decimal bytes:
 *
 *     <struct> <field> <offset> <size>
 *     <struct> sizeof <size>
 *
 * A struct's field lines come in declaration order and are followed by its sizeof line. A field of struct type is one
 * line; that struct is listed by its own name. An opaque struct has no layout and is not listed.
 */

static void print_field(const char *type, const char *field, const void *instance, const void *member,
                        size_t size) {
    printf("%s %s %zu %zu\n", type, field, (size_t)((const char *)member - (const char *)instance), size);
}

static void print_size(const char *type, size_t size) {
    printf("%s sizeof %zu\n", type, size);
}

#define FIELD(type, member)                                                                  \
    do {                                                                                     \
        static type instance;                                                                \
        print_field(#type, #member, &instance, &instance.member, sizeof instance.member);    \
    } while (0)

#define SIZE(type) print_size(#type, sizeof(type))

static void print_discovery_layouts(void) {
    FIELD(weivel_standard_identity, size);
    FIELD(weivel_standard_identity, kind);
    FIELD(weivel_standard_identity, name);
    FIELD(weivel_standard_identity, name_length);
    FIELD(weivel_standard_identity, forge);
    FIELD(weivel_standard_identity, host);
    FIELD(weivel_standard_identity, number);
    FIELD(weivel_standard_identity, segments);
    FIELD(weivel_standard_identity, segment_count);
    SIZE(weivel_standard_identity);
    FIELD(weivel_standard_discovered_plugin, manifest);
    FIELD(weivel_standard_discovered_plugin, manifest_path);
    FIELD(weivel_standard_discovered_plugin, identity_path);
    FIELD(weivel_standard_discovered_plugin, identity_id);
    FIELD(weivel_standard_discovered_plugin, repository_name);
    FIELD(weivel_standard_discovered_plugin, identity);
    FIELD(weivel_standard_discovered_plugin, entry_path);
    FIELD(weivel_standard_discovered_plugin, entry_present);
    SIZE(weivel_standard_discovered_plugin);
    FIELD(weivel_standard_discovery_failure, file);
    FIELD(weivel_standard_discovery_failure, refusal);
    SIZE(weivel_standard_discovery_failure);
    FIELD(weivel_standard_discovery, size);
    FIELD(weivel_standard_discovery, plugins);
    FIELD(weivel_standard_discovery, plugin_count);
    FIELD(weivel_standard_discovery, failures);
    FIELD(weivel_standard_discovery, failure_count);
    FIELD(weivel_standard_discovery, refusal);
    SIZE(weivel_standard_discovery);
}

static void print_library_layouts(void) {
    FIELD(weivel_standard_library_function, name);
    FIELD(weivel_standard_library_function, symbol);
    FIELD(weivel_standard_library_function, kind);
    FIELD(weivel_standard_library_function, context);
    SIZE(weivel_standard_library_function);
    FIELD(weivel_standard_library_service, id);
    FIELD(weivel_standard_library_service, functions);
    FIELD(weivel_standard_library_service, function_count);
    SIZE(weivel_standard_library_service);
    FIELD(weivel_standard_library, size);
    FIELD(weivel_standard_library, import_name);
    FIELD(weivel_standard_library, version);
    FIELD(weivel_standard_library, release);
    FIELD(weivel_standard_library, services);
    FIELD(weivel_standard_library, service_count);
    FIELD(weivel_standard_library, storage);
    SIZE(weivel_standard_library);
}

static void print_service_layouts(void) {
    FIELD(weivel_standard_mirror_list, size);
    FIELD(weivel_standard_mirror_list, templates);
    FIELD(weivel_standard_mirror_list, count);
    FIELD(weivel_standard_mirror_list, storage);
    SIZE(weivel_standard_mirror_list);
    FIELD(weivel_standard_service_refusal, size);
    FIELD(weivel_standard_service_refusal, detail);
    FIELD(weivel_standard_service_refusal, fix);
    FIELD(weivel_standard_service_refusal, storage);
    SIZE(weivel_standard_service_refusal);
    FIELD(weivel_standard_setting_value, kind);
    FIELD(weivel_standard_setting_value, boolean);
    FIELD(weivel_standard_setting_value, number);
    FIELD(weivel_standard_setting_value, text);
    FIELD(weivel_standard_setting_value, text_length);
    SIZE(weivel_standard_setting_value);
    FIELD(weivel_standard_setting, key);
    FIELD(weivel_standard_setting, value);
    SIZE(weivel_standard_setting);
    FIELD(weivel_standard_settings, size);
    FIELD(weivel_standard_settings, settings);
    FIELD(weivel_standard_settings, count);
    FIELD(weivel_standard_settings, storage);
    SIZE(weivel_standard_settings);
    FIELD(weivel_standard_setting_answer, size);
    FIELD(weivel_standard_setting_answer, value);
    FIELD(weivel_standard_setting_answer, storage);
    SIZE(weivel_standard_setting_answer);
    FIELD(weivel_standard_config, size);
    FIELD(weivel_standard_config, context);
    FIELD(weivel_standard_config, all);
    FIELD(weivel_standard_config, get);
    FIELD(weivel_standard_config, set);
    FIELD(weivel_standard_config, release_settings);
    FIELD(weivel_standard_config, release_answer);
    FIELD(weivel_standard_config, release_refusal);
    SIZE(weivel_standard_config);
    FIELD(weivel_standard_secret, size);
    FIELD(weivel_standard_secret, bytes);
    FIELD(weivel_standard_secret, length);
    FIELD(weivel_standard_secret, storage);
    SIZE(weivel_standard_secret);
    FIELD(weivel_standard_secrets, size);
    FIELD(weivel_standard_secrets, context);
    FIELD(weivel_standard_secrets, remove);
    FIELD(weivel_standard_secrets, get);
    FIELD(weivel_standard_secrets, set);
    FIELD(weivel_standard_secrets, release_secret);
    FIELD(weivel_standard_secrets, release_refusal);
    SIZE(weivel_standard_secrets);
    FIELD(weivel_standard_paths, size);
    FIELD(weivel_standard_paths, context);
    FIELD(weivel_standard_paths, cache);
    FIELD(weivel_standard_paths, config);
    FIELD(weivel_standard_paths, plugin);
    SIZE(weivel_standard_paths);
}

static void print_validation_layouts(void) {
    FIELD(weivel_standard_refusal, size);
    FIELD(weivel_standard_refusal, code);
    FIELD(weivel_standard_refusal, detail);
    FIELD(weivel_standard_refusal, fix);
    FIELD(weivel_standard_refusal, path);
    FIELD(weivel_standard_refusal, storage);
    SIZE(weivel_standard_refusal);
    FIELD(weivel_standard_validation_options, size);
    FIELD(weivel_standard_validation_options, well_known_services);
    SIZE(weivel_standard_validation_options);
    FIELD(weivel_standard_validation, size);
    FIELD(weivel_standard_validation, valid);
    FIELD(weivel_standard_validation, paths);
    FIELD(weivel_standard_validation, refusal);
    FIELD(weivel_standard_validation, storage);
    SIZE(weivel_standard_validation);
}

int main(void) {
    print_discovery_layouts();
    print_library_layouts();
    print_service_layouts();
    print_validation_layouts();
    return fflush(stdout) == 0 ? 0 : 1;
}
