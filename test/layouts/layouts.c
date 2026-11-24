#include <stdio.h>

#include "weivel/description.h"
#include "weivel/host.h"
#include "weivel/identity.h"
#include "weivel/json.h"
#include "weivel/layout.h"
#include "weivel/library.h"
#include "weivel/mirrors.h"
#include "weivel/scope.h"
#include "weivel/services.h"
#include "weivel/standard.h"
#include "weivel/value.h"

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
 * line; that struct is listed by its own name. An opaque struct has no layout and is not listed. A struct a header
 * declares only by its typedef name, as json.h does weivel_json_error, is listed by that name.
 *
 * An array field is one line carrying the array's total size and no element count, as `weivel_json_error detail`
 * does, so a consumer takes the element type and count from its own declaration. Lines end in CRLF on Windows, where
 * stdout is a text stream, and in LF elsewhere, so a consumer strips a trailing carriage return before splitting.
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
    FIELD(weivel_standard_library_contract, capability);
    FIELD(weivel_standard_library_contract, description);
    SIZE(weivel_standard_library_contract);
    FIELD(weivel_standard_library, size);
    FIELD(weivel_standard_library, import_name);
    FIELD(weivel_standard_library, version);
    FIELD(weivel_standard_library, release);
    FIELD(weivel_standard_library, services);
    FIELD(weivel_standard_library, service_count);
    FIELD(weivel_standard_library, storage);
    FIELD(weivel_standard_library, descriptions);
    FIELD(weivel_standard_library, contracts);
    FIELD(weivel_standard_library, contract_count);
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
    FIELD(weivel_standard_settings_file, size);
    FIELD(weivel_standard_settings_file, path);
    FIELD(weivel_standard_settings_file, storage);
    SIZE(weivel_standard_settings_file);
    FIELD(weivel_standard_config, size);
    FIELD(weivel_standard_config, context);
    FIELD(weivel_standard_config, all);
    FIELD(weivel_standard_config, get);
    FIELD(weivel_standard_config, set);
    FIELD(weivel_standard_config, release_settings);
    FIELD(weivel_standard_config, release_answer);
    FIELD(weivel_standard_config, release_refusal);
    FIELD(weivel_standard_config, get_tree);
    FIELD(weivel_standard_config, release_tree);
    FIELD(weivel_standard_config, settings_file);
    FIELD(weivel_standard_config, release_settings_file);
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
    FIELD(weivel_standard_log, size);
    FIELD(weivel_standard_log, context);
    FIELD(weivel_standard_log, write);
    FIELD(weivel_standard_log, release_refusal);
    SIZE(weivel_standard_log);
    FIELD(weivel_standard_scope, size);
    FIELD(weivel_standard_scope, config);
    FIELD(weivel_standard_scope, secrets);
    FIELD(weivel_standard_scope, paths);
    FIELD(weivel_standard_scope, log);
    FIELD(weivel_standard_scope, storage);
    SIZE(weivel_standard_scope);
    FIELD(weivel_standard_scopes, size);
    FIELD(weivel_standard_scopes, context);
    FIELD(weivel_standard_scopes, open);
    FIELD(weivel_standard_scopes, release_scope);
    FIELD(weivel_standard_scopes, release_refusal);
    SIZE(weivel_standard_scopes);
}

static void print_host_layouts(void) {
    FIELD(weivel_standard_host_library, binary);
    FIELD(weivel_standard_host_library, declaration);
    SIZE(weivel_standard_host_library);
    FIELD(weivel_standard_host_plugin, directory);
    FIELD(weivel_standard_host_plugin, manifest);
    FIELD(weivel_standard_host_plugin, entry);
    SIZE(weivel_standard_host_plugin);
    FIELD(weivel_standard_host_plan, size);
    FIELD(weivel_standard_host_plan, mode);
    FIELD(weivel_standard_host_plan, libraries);
    FIELD(weivel_standard_host_plan, library_count);
    FIELD(weivel_standard_host_plan, plugins);
    FIELD(weivel_standard_host_plan, plugin_count);
    FIELD(weivel_standard_host_plan, action_plugin);
    FIELD(weivel_standard_host_plan, action_name);
    FIELD(weivel_standard_host_plan, action_arguments);
    FIELD(weivel_standard_host_plan, storage);
    FIELD(weivel_standard_host_plan, action_service);
    FIELD(weivel_standard_host_plan, action_method);
    SIZE(weivel_standard_host_plan);
    FIELD(weivel_standard_host_outcome, manifest);
    FIELD(weivel_standard_host_outcome, state);
    FIELD(weivel_standard_host_outcome, detail);
    SIZE(weivel_standard_host_outcome);
    FIELD(weivel_standard_host_next, size);
    FIELD(weivel_standard_host_next, has_next);
    FIELD(weivel_standard_host_next, next);
    FIELD(weivel_standard_host_next, storage);
    SIZE(weivel_standard_host_next);
    FIELD(weivel_standard_host, size);
    FIELD(weivel_standard_host, context);
    FIELD(weivel_standard_host, start);
    FIELD(weivel_standard_host, started);
    FIELD(weivel_standard_host, tick);
    FIELD(weivel_standard_host, finished);
    FIELD(weivel_standard_host, stop);
    FIELD(weivel_standard_host, release_plan);
    FIELD(weivel_standard_host, release_next);
    FIELD(weivel_standard_host, release_refusal);
    SIZE(weivel_standard_host);
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

static void print_description_layouts(void) {
    FIELD(weivel_standard_described_member, name);
    FIELD(weivel_standard_described_member, offset);
    FIELD(weivel_standard_described_member, size);
    FIELD(weivel_standard_described_member, kind);
    FIELD(weivel_standard_described_member, element);
    FIELD(weivel_standard_described_member, type);
    FIELD(weivel_standard_described_member, count);
    FIELD(weivel_standard_described_member, release);
    SIZE(weivel_standard_described_member);
    FIELD(weivel_standard_described_type, name);
    FIELD(weivel_standard_described_type, size);
    FIELD(weivel_standard_described_type, first_shape);
    FIELD(weivel_standard_described_type, members);
    FIELD(weivel_standard_described_type, member_count);
    SIZE(weivel_standard_described_type);
    FIELD(weivel_standard_described_enum_entry, name);
    FIELD(weivel_standard_described_enum_entry, number);
    SIZE(weivel_standard_described_enum_entry);
    FIELD(weivel_standard_described_enum, name);
    FIELD(weivel_standard_described_enum, entries);
    FIELD(weivel_standard_described_enum, entry_count);
    SIZE(weivel_standard_described_enum);
    FIELD(weivel_standard_described_parameter, name);
    FIELD(weivel_standard_described_parameter, kind);
    FIELD(weivel_standard_described_parameter, role);
    FIELD(weivel_standard_described_parameter, element);
    FIELD(weivel_standard_described_parameter, type);
    FIELD(weivel_standard_described_parameter, count);
    FIELD(weivel_standard_described_parameter, release);
    SIZE(weivel_standard_described_parameter);
    FIELD(weivel_standard_described_operation, name);
    FIELD(weivel_standard_described_operation, offset);
    FIELD(weivel_standard_described_operation, returns);
    FIELD(weivel_standard_described_operation, parameters);
    FIELD(weivel_standard_described_operation, parameter_count);
    SIZE(weivel_standard_described_operation);
    FIELD(weivel_standard_description, size);
    FIELD(weivel_standard_description, definition);
    FIELD(weivel_standard_description, types);
    FIELD(weivel_standard_description, type_count);
    FIELD(weivel_standard_description, enums);
    FIELD(weivel_standard_description, enum_count);
    FIELD(weivel_standard_description, operations);
    FIELD(weivel_standard_description, operation_count);
    FIELD(weivel_standard_description, blocking);
    FIELD(weivel_standard_description, blocking_count);
    FIELD(weivel_standard_description, definitions);
    FIELD(weivel_standard_description, definition_count);
    FIELD(weivel_standard_description, answered_by);
    SIZE(weivel_standard_description);
}

static void print_value_layouts(void) {
    FIELD(weivel_standard_value, kind);
    FIELD(weivel_standard_value, boolean);
    FIELD(weivel_standard_value, number);
    FIELD(weivel_standard_value, text);
    FIELD(weivel_standard_value, length);
    FIELD(weivel_standard_value, items);
    FIELD(weivel_standard_value, entries);
    FIELD(weivel_standard_value, count);
    SIZE(weivel_standard_value);
    FIELD(weivel_standard_entry, key);
    FIELD(weivel_standard_entry, key_length);
    FIELD(weivel_standard_entry, value);
    SIZE(weivel_standard_entry);
    FIELD(weivel_standard_value_tree, size);
    FIELD(weivel_standard_value_tree, root);
    FIELD(weivel_standard_value_tree, storage);
    SIZE(weivel_standard_value_tree);
}

static void print_json_layouts(void) {
    FIELD(weivel_json_error, size);
    FIELD(weivel_json_error, line);
    FIELD(weivel_json_error, column);
    FIELD(weivel_json_error, message);
    FIELD(weivel_json_error, detail);
    FIELD(weivel_json_error, out_of_memory);
    SIZE(weivel_json_error);
}

int main(void) {
    print_json_layouts();
    print_discovery_layouts();
    print_library_layouts();
    print_description_layouts();
    print_value_layouts();
    print_service_layouts();
    print_host_layouts();
    print_validation_layouts();
    return fflush(stdout) == 0 ? 0 : 1;
}
