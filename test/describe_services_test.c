/**
 * Walks every description the standard ships against the layout this compile gives its headers: each expectation
 * below is written by hand from the header, with `offsetof` and `sizeof`, never read from the generated source.
 */
#include <stdio.h>
#include <string.h>

#include "check.h"
#include "weivel/description.h"
#include "weivel/descriptions.h"
#include "weivel/host.h"
#include "weivel/scope.h"
#include "weivel/services.h"

#define K(name) WEIVEL_STANDARD_DESCRIBED_##name
#define COUNT(array) (sizeof(array) / sizeof((array)[0]))
#define FIELD_END(type, field) (offsetof(type, field) + sizeof(((type *)0)->field))
#define MEMBER(type, field, kind, element, named, count) \
    {#field, offsetof(type, field), sizeof(((type *)0)->field), K(kind), K(element), named, count}
#define PARAMETER(name, kind, role, element, named, count, release) \
    {name, K(kind), K(role), K(element), named, count, release}
#define CONTEXT_PARAMETER PARAMETER("context", CONTEXT, INPUT, NONE, NULL, NULL, NULL)
#define REFUSAL_PARAMETER \
    PARAMETER("refusal", POINTER, REFUSAL, STRUCT, "weivel_standard_service_refusal", NULL, "release_refusal")
#define RELEASED_REFUSAL PARAMETER("refusal", POINTER, REFUSAL, STRUCT, "weivel_standard_service_refusal", NULL, NULL)
#define OPERATION(definition, name, returns, parameters) \
    {#name, offsetof(definition, name), K(returns), parameters, COUNT(parameters)}
#define TYPE(type, first_shape, members) {#type, sizeof(type), first_shape, members, COUNT(members)}

typedef struct expected_member {
    const char *name;
    size_t offset;
    size_t size;
    uint32_t kind;
    uint32_t element;
    const char *type;
    const char *count;
} expected_member;

typedef struct expected_type {
    const char *name;
    size_t size;
    size_t first_shape;
    const expected_member *members;
    size_t member_count;
} expected_type;

typedef struct expected_parameter {
    const char *name;
    uint32_t kind;
    uint32_t role;
    uint32_t element;
    const char *type;
    const char *count;
    const char *release;
} expected_parameter;

typedef struct expected_operation {
    const char *name;
    size_t offset;
    uint32_t returns;
    const expected_parameter *parameters;
    size_t parameter_count;
} expected_operation;

typedef struct expected_enum {
    const char *name;
    const weivel_standard_described_enum_entry *entries;
    size_t entry_count;
} expected_enum;

typedef struct expected_description {
    const char *definition;
    const expected_type *types;
    size_t type_count;
    const expected_enum *enums;
    size_t enum_count;
    const expected_operation *operations;
    size_t operation_count;
    const char *const *blocking;
    size_t blocking_count;
} expected_description;

static int same_text(const char *actual, const char *expected) {
    return actual == expected || (actual != NULL && expected != NULL && strcmp(actual, expected) == 0);
}

static const char *shown(const char *text) {
    return text == NULL ? "(null)" : text;
}

static void walk_member(const char *type, const weivel_standard_described_member *actual,
                        const expected_member *expected) {
    if (!same_text(actual->name, expected->name) || actual->offset != expected->offset ||
        actual->size != expected->size || actual->kind != expected->kind || actual->element != expected->element ||
        !same_text(actual->type, expected->type) || !same_text(actual->count, expected->count) ||
        actual->release != NULL) {
        fprintf(stderr, "%s.%s: expected offset %u size %u kind %u element %u type %s count %s; got %s at %u size %u "
                        "kind %u element %u type %s count %s release %s\n",
                type, expected->name, (unsigned)expected->offset, (unsigned)expected->size, (unsigned)expected->kind,
                (unsigned)expected->element, shown(expected->type), shown(expected->count), shown(actual->name),
                (unsigned)actual->offset, (unsigned)actual->size, (unsigned)actual->kind, (unsigned)actual->element,
                shown(actual->type), shown(actual->count), shown(actual->release));
        check_failures++;
    }
}

static const weivel_standard_described_type *type_named(const weivel_standard_description *description,
                                                        const char *name) {
    size_t at;
    for (at = 0; at < description->type_count; at++) {
        if (same_text(description->types[at].name, name)) {
            return &description->types[at];
        }
    }
    return NULL;
}

static void walk_type(const weivel_standard_description *description, const expected_type *expected) {
    const weivel_standard_described_type *actual = type_named(description, expected->name);
    size_t at;
    if (actual == NULL) {
        fprintf(stderr, "%s: no type %s\n", description->definition, expected->name);
        check_failures++;
        return;
    }
    if (actual->size != expected->size || actual->first_shape != expected->first_shape ||
        actual->member_count != expected->member_count) {
        fprintf(stderr, "%s: expected size %u, first shape %u, %u members; got %u, %u, %u\n", expected->name,
                (unsigned)expected->size, (unsigned)expected->first_shape, (unsigned)expected->member_count,
                (unsigned)actual->size, (unsigned)actual->first_shape, (unsigned)actual->member_count);
        check_failures++;
        return;
    }
    for (at = 0; at < expected->member_count; at++) {
        walk_member(expected->name, &actual->members[at], &expected->members[at]);
    }
}

static void walk_enum(const weivel_standard_described_enum *actual, const expected_enum *expected) {
    size_t at;
    if (!same_text(actual->name, expected->name) || actual->entry_count != expected->entry_count) {
        fprintf(stderr, "expected enum %s of %u entries, got %s of %u\n", expected->name,
                (unsigned)expected->entry_count, shown(actual->name), (unsigned)actual->entry_count);
        check_failures++;
        return;
    }
    for (at = 0; at < expected->entry_count; at++) {
        if (!same_text(actual->entries[at].name, expected->entries[at].name) ||
            actual->entries[at].number != expected->entries[at].number) {
            fprintf(stderr, "%s: expected %s = %u, got %s = %u\n", expected->name, expected->entries[at].name,
                    (unsigned)expected->entries[at].number, shown(actual->entries[at].name),
                    (unsigned)actual->entries[at].number);
            check_failures++;
        }
    }
}

static void walk_parameter(const char *operation, const weivel_standard_described_parameter *actual,
                           const expected_parameter *expected) {
    if (!same_text(actual->name, expected->name) || actual->kind != expected->kind ||
        actual->role != expected->role || actual->element != expected->element ||
        !same_text(actual->type, expected->type) || !same_text(actual->count, expected->count) ||
        !same_text(actual->release, expected->release)) {
        fprintf(stderr, "%s(%s): expected kind %u role %u element %u type %s count %s release %s; got %s kind %u "
                        "role %u element %u type %s count %s release %s\n",
                operation, expected->name, (unsigned)expected->kind, (unsigned)expected->role,
                (unsigned)expected->element, shown(expected->type), shown(expected->count), shown(expected->release),
                shown(actual->name), (unsigned)actual->kind, (unsigned)actual->role, (unsigned)actual->element,
                shown(actual->type), shown(actual->count), shown(actual->release));
        check_failures++;
    }
}

static void walk_operation(const weivel_standard_described_operation *actual, const expected_operation *expected) {
    size_t at;
    if (!same_text(actual->name, expected->name) || actual->offset != expected->offset ||
        actual->returns != expected->returns || actual->parameter_count != expected->parameter_count) {
        fprintf(stderr, "expected operation %s at %u returning %u with %u parameters; got %s at %u, %u, %u\n",
                expected->name, (unsigned)expected->offset, (unsigned)expected->returns,
                (unsigned)expected->parameter_count, shown(actual->name), (unsigned)actual->offset,
                (unsigned)actual->returns, (unsigned)actual->parameter_count);
        check_failures++;
        return;
    }
    for (at = 0; at < expected->parameter_count; at++) {
        walk_parameter(expected->name, &actual->parameters[at], &expected->parameters[at]);
    }
}

static int passes_the_check(const weivel_standard_description *description) {
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    if (description == NULL) {
        fprintf(stderr, "a symbol answered no description\n");
        check_failures++;
        return 0;
    }
    if (weivel_standard_description_check(description, &refusal) != WEIVEL_STATUS_OK) {
        fprintf(stderr, "%s: refused by the check at %s: %s\n", shown(description->definition), shown(refusal.path),
                shown(refusal.detail));
        weivel_standard_refusal_release(&refusal);
        check_failures++;
        return 0;
    }
    CHECK(description->size == sizeof(weivel_standard_description));
    return 1;
}

/** Walks `description` against `expected`, answering whether it passed the check, so its nesting can be read next. */
static int walk(const weivel_standard_description *description, const expected_description *expected) {
    size_t at;
    if (!passes_the_check(description)) {
        return 0;
    }
    check_text(description->definition, expected->definition, "definition");
    if (description->type_count != expected->type_count) {
        fprintf(stderr, "%s: expected %u types, got %u\n", expected->definition, (unsigned)expected->type_count,
                (unsigned)description->type_count);
        check_failures++;
    }
    for (at = 0; at < expected->type_count; at++) {
        walk_type(description, &expected->types[at]);
    }
    if (description->enum_count != expected->enum_count) {
        fprintf(stderr, "%s: expected %u enums, got %u\n", expected->definition, (unsigned)expected->enum_count,
                (unsigned)description->enum_count);
        check_failures++;
    } else {
        for (at = 0; at < expected->enum_count; at++) {
            walk_enum(&description->enums[at], &expected->enums[at]);
        }
    }
    if (description->operation_count != expected->operation_count) {
        fprintf(stderr, "%s: expected %u operations, got %u\n", expected->definition,
                (unsigned)expected->operation_count, (unsigned)description->operation_count);
        check_failures++;
    } else {
        for (at = 0; at < expected->operation_count; at++) {
            walk_operation(&description->operations[at], &expected->operations[at]);
        }
    }
    if (description->blocking_count != expected->blocking_count) {
        fprintf(stderr, "%s: expected %u blocking operations, got %u\n", expected->definition,
                (unsigned)expected->blocking_count, (unsigned)description->blocking_count);
        check_failures++;
    } else {
        for (at = 0; at < expected->blocking_count; at++) {
            check_text(description->blocking[at], expected->blocking[at], "blocking");
        }
    }
    return 1;
}

static const expected_member REFUSAL_MEMBERS[] = {
    MEMBER(weivel_standard_service_refusal, size, U32, NONE, NULL, NULL),
    MEMBER(weivel_standard_service_refusal, detail, TEXT, NONE, NULL, NULL),
    MEMBER(weivel_standard_service_refusal, fix, TEXT, NONE, NULL, NULL),
    MEMBER(weivel_standard_service_refusal, storage, STORAGE, NONE, NULL, NULL)};
#define REFUSAL_TYPE \
    TYPE(weivel_standard_service_refusal, FIELD_END(weivel_standard_service_refusal, storage), REFUSAL_MEMBERS)

static const expected_member CONFIG_MEMBERS[] = {MEMBER(weivel_standard_config, size, U32, NONE, NULL, NULL),
                                                 MEMBER(weivel_standard_config, context, CONTEXT, NONE, NULL, NULL)};
static const expected_member SETTING_VALUE_MEMBERS[] = {
    MEMBER(weivel_standard_setting_value, kind, U32, NONE, "weivel_standard_setting_kind", NULL),
    MEMBER(weivel_standard_setting_value, boolean, U32, NONE, NULL, NULL),
    MEMBER(weivel_standard_setting_value, number, F64, NONE, NULL, NULL),
    MEMBER(weivel_standard_setting_value, text, TEXT, NONE, NULL, NULL),
    MEMBER(weivel_standard_setting_value, text_length, SIZE, NONE, NULL, NULL)};
static const expected_member SETTING_MEMBERS[] = {
    MEMBER(weivel_standard_setting, key, TEXT, NONE, NULL, NULL),
    MEMBER(weivel_standard_setting, value, STRUCT, NONE, "weivel_standard_setting_value", NULL)};
static const expected_member SETTINGS_MEMBERS[] = {
    MEMBER(weivel_standard_settings, size, U32, NONE, NULL, NULL),
    MEMBER(weivel_standard_settings, settings, ARRAY, STRUCT, "weivel_standard_setting", "count"),
    MEMBER(weivel_standard_settings, count, SIZE, NONE, NULL, NULL),
    MEMBER(weivel_standard_settings, storage, STORAGE, NONE, NULL, NULL)};
static const expected_member SETTING_ANSWER_MEMBERS[] = {
    MEMBER(weivel_standard_setting_answer, size, U32, NONE, NULL, NULL),
    MEMBER(weivel_standard_setting_answer, value, STRUCT, NONE, "weivel_standard_setting_value", NULL),
    MEMBER(weivel_standard_setting_answer, storage, STORAGE, NONE, NULL, NULL)};
static const expected_type CONFIG_TYPES[] = {
    TYPE(weivel_standard_config, FIELD_END(weivel_standard_config, release_refusal), CONFIG_MEMBERS),
    REFUSAL_TYPE,
    TYPE(weivel_standard_setting_value, sizeof(weivel_standard_setting_value), SETTING_VALUE_MEMBERS),
    TYPE(weivel_standard_setting, sizeof(weivel_standard_setting), SETTING_MEMBERS),
    TYPE(weivel_standard_settings, FIELD_END(weivel_standard_settings, storage), SETTINGS_MEMBERS),
    TYPE(weivel_standard_setting_answer, FIELD_END(weivel_standard_setting_answer, storage), SETTING_ANSWER_MEMBERS)};
static const weivel_standard_described_enum_entry SETTING_KINDS[] = {
    {"WEIVEL_STANDARD_SETTING_NONE", 0},
    {"WEIVEL_STANDARD_SETTING_TEXT", 1},
    {"WEIVEL_STANDARD_SETTING_NUMBER", 2},
    {"WEIVEL_STANDARD_SETTING_BOOLEAN", 3}};
static const expected_enum CONFIG_ENUMS[] = {{"weivel_standard_setting_kind", SETTING_KINDS, COUNT(SETTING_KINDS)}};
static const expected_parameter CONFIG_ALL[] = {
    CONTEXT_PARAMETER,
    PARAMETER("settings", POINTER, ANSWER, STRUCT, "weivel_standard_settings", NULL, "release_settings"),
    REFUSAL_PARAMETER};
static const expected_parameter CONFIG_GET[] = {
    CONTEXT_PARAMETER, PARAMETER("key", TEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("answer", POINTER, ANSWER, STRUCT, "weivel_standard_setting_answer", NULL, "release_answer"),
    REFUSAL_PARAMETER};
static const expected_parameter CONFIG_SET[] = {
    CONTEXT_PARAMETER, PARAMETER("key", TEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("value", POINTER, INPUT, STRUCT, "weivel_standard_setting_value", NULL, NULL), REFUSAL_PARAMETER};
static const expected_parameter CONFIG_RELEASE_SETTINGS[] = {
    CONTEXT_PARAMETER, PARAMETER("settings", POINTER, ANSWER, STRUCT, "weivel_standard_settings", NULL, NULL)};
static const expected_parameter CONFIG_RELEASE_ANSWER[] = {
    CONTEXT_PARAMETER, PARAMETER("answer", POINTER, ANSWER, STRUCT, "weivel_standard_setting_answer", NULL, NULL)};
static const expected_parameter RELEASE_REFUSAL[] = {CONTEXT_PARAMETER, RELEASED_REFUSAL};
static const expected_operation CONFIG_OPERATIONS[] = {
    OPERATION(weivel_standard_config, all, STATUS, CONFIG_ALL),
    OPERATION(weivel_standard_config, get, STATUS, CONFIG_GET),
    OPERATION(weivel_standard_config, set, STATUS, CONFIG_SET),
    OPERATION(weivel_standard_config, release_settings, NOTHING, CONFIG_RELEASE_SETTINGS),
    OPERATION(weivel_standard_config, release_answer, NOTHING, CONFIG_RELEASE_ANSWER),
    OPERATION(weivel_standard_config, release_refusal, NOTHING, RELEASE_REFUSAL)};
static const expected_description CONFIG = {"weivel_standard_config", CONFIG_TYPES, COUNT(CONFIG_TYPES),
                                            CONFIG_ENUMS, COUNT(CONFIG_ENUMS), CONFIG_OPERATIONS,
                                            COUNT(CONFIG_OPERATIONS), NULL, 0};

static const expected_member SECRETS_MEMBERS[] = {
    MEMBER(weivel_standard_secrets, size, U32, NONE, NULL, NULL),
    MEMBER(weivel_standard_secrets, context, CONTEXT, NONE, NULL, NULL)};
static const expected_member SECRET_MEMBERS[] = {MEMBER(weivel_standard_secret, size, U32, NONE, NULL, NULL),
                                                 MEMBER(weivel_standard_secret, bytes, BYTES, NONE, NULL, "length"),
                                                 MEMBER(weivel_standard_secret, length, SIZE, NONE, NULL, NULL),
                                                 MEMBER(weivel_standard_secret, storage, STORAGE, NONE, NULL, NULL)};
static const expected_type SECRETS_TYPES[] = {
    TYPE(weivel_standard_secrets, FIELD_END(weivel_standard_secrets, release_refusal), SECRETS_MEMBERS),
    REFUSAL_TYPE, TYPE(weivel_standard_secret, FIELD_END(weivel_standard_secret, storage), SECRET_MEMBERS)};
static const expected_parameter SECRETS_REMOVE[] = {CONTEXT_PARAMETER,
                                                    PARAMETER("name", TEXT, INPUT, NONE, NULL, NULL, NULL),
                                                    REFUSAL_PARAMETER};
static const expected_parameter SECRETS_GET[] = {
    CONTEXT_PARAMETER, PARAMETER("name", TEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("secret", POINTER, ANSWER, STRUCT, "weivel_standard_secret", NULL, "release_secret"),
    REFUSAL_PARAMETER};
static const expected_parameter SECRETS_SET[] = {
    CONTEXT_PARAMETER, PARAMETER("name", TEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("bytes", BYTES, INPUT, NONE, NULL, "length", NULL),
    PARAMETER("length", SIZE, INPUT, NONE, NULL, NULL, NULL),
    REFUSAL_PARAMETER};
static const expected_parameter SECRETS_RELEASE_SECRET[] = {
    CONTEXT_PARAMETER, PARAMETER("secret", POINTER, ANSWER, STRUCT, "weivel_standard_secret", NULL, NULL)};
static const expected_operation SECRETS_OPERATIONS[] = {
    OPERATION(weivel_standard_secrets, remove, STATUS, SECRETS_REMOVE),
    OPERATION(weivel_standard_secrets, get, STATUS, SECRETS_GET),
    OPERATION(weivel_standard_secrets, set, STATUS, SECRETS_SET),
    OPERATION(weivel_standard_secrets, release_secret, NOTHING, SECRETS_RELEASE_SECRET),
    OPERATION(weivel_standard_secrets, release_refusal, NOTHING, RELEASE_REFUSAL)};
static const expected_description SECRETS = {"weivel_standard_secrets", SECRETS_TYPES, COUNT(SECRETS_TYPES), NULL, 0,
                                             SECRETS_OPERATIONS, COUNT(SECRETS_OPERATIONS), NULL, 0};

static const expected_member PATHS_MEMBERS[] = {MEMBER(weivel_standard_paths, size, U32, NONE, NULL, NULL),
                                                MEMBER(weivel_standard_paths, context, CONTEXT, NONE, NULL, NULL)};
static const expected_type PATHS_TYPES[] = {
    TYPE(weivel_standard_paths, FIELD_END(weivel_standard_paths, plugin), PATHS_MEMBERS)};
static const expected_parameter PATHS_DIRECTORY[] = {CONTEXT_PARAMETER,
                                                     PARAMETER("directory", TEXT, ANSWER, NONE, NULL, NULL, NULL)};
static const expected_operation PATHS_OPERATIONS[] = {
    OPERATION(weivel_standard_paths, cache, STATUS, PATHS_DIRECTORY),
    OPERATION(weivel_standard_paths, config, STATUS, PATHS_DIRECTORY),
    OPERATION(weivel_standard_paths, plugin, STATUS, PATHS_DIRECTORY)};
static const expected_description PATHS = {"weivel_standard_paths", PATHS_TYPES, COUNT(PATHS_TYPES), NULL, 0,
                                           PATHS_OPERATIONS, COUNT(PATHS_OPERATIONS), NULL, 0};

static const expected_member LOG_MEMBERS[] = {MEMBER(weivel_standard_log, size, U32, NONE, NULL, NULL),
                                              MEMBER(weivel_standard_log, context, CONTEXT, NONE, NULL, NULL)};
static const expected_type LOG_TYPES[] = {
    TYPE(weivel_standard_log, FIELD_END(weivel_standard_log, release_refusal), LOG_MEMBERS), REFUSAL_TYPE};
static const weivel_standard_described_enum_entry LOG_LEVELS[] = {
    {"WEIVEL_STANDARD_LOG_TRACE", 1},
    {"WEIVEL_STANDARD_LOG_DEBUG", 2},
    {"WEIVEL_STANDARD_LOG_INFO", 3},
    {"WEIVEL_STANDARD_LOG_WARN", 4},
    {"WEIVEL_STANDARD_LOG_ERROR", 5}};
static const expected_enum LOG_ENUMS[] = {{"weivel_standard_log_level", LOG_LEVELS, COUNT(LOG_LEVELS)}};
static const expected_parameter LOG_WRITE[] = {
    CONTEXT_PARAMETER, PARAMETER("level", U32, INPUT, NONE, "weivel_standard_log_level", NULL, NULL),
    PARAMETER("message", TEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("fields", POINTER, INPUT, VALUE, NULL, NULL, NULL), REFUSAL_PARAMETER};
static const expected_operation LOG_OPERATIONS[] = {
    OPERATION(weivel_standard_log, write, STATUS, LOG_WRITE),
    OPERATION(weivel_standard_log, release_refusal, NOTHING, RELEASE_REFUSAL)};
static const expected_description LOG = {"weivel_standard_log", LOG_TYPES, COUNT(LOG_TYPES), LOG_ENUMS,
                                         COUNT(LOG_ENUMS), LOG_OPERATIONS, COUNT(LOG_OPERATIONS), NULL, 0};

static const expected_member SCOPES_MEMBERS[] = {MEMBER(weivel_standard_scopes, size, U32, NONE, NULL, NULL),
                                                 MEMBER(weivel_standard_scopes, context, CONTEXT, NONE, NULL, NULL)};
static const expected_member SCOPE_MEMBERS[] = {
    MEMBER(weivel_standard_scope, size, U32, NONE, NULL, NULL),
    MEMBER(weivel_standard_scope, config, DEFINITION, NONE, "weivel_standard_config", NULL),
    MEMBER(weivel_standard_scope, secrets, DEFINITION, NONE, "weivel_standard_secrets", NULL),
    MEMBER(weivel_standard_scope, paths, DEFINITION, NONE, "weivel_standard_paths", NULL),
    MEMBER(weivel_standard_scope, log, DEFINITION, NONE, "weivel_standard_log", NULL),
    MEMBER(weivel_standard_scope, storage, STORAGE, NONE, NULL, NULL)};
static const expected_type SCOPES_TYPES[] = {
    TYPE(weivel_standard_scopes, FIELD_END(weivel_standard_scopes, release_refusal), SCOPES_MEMBERS), REFUSAL_TYPE,
    TYPE(weivel_standard_scope, FIELD_END(weivel_standard_scope, storage), SCOPE_MEMBERS)};
static const expected_parameter SCOPES_OPEN[] = {
    CONTEXT_PARAMETER, PARAMETER("manifest_path", TEXT, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("scope", POINTER, ANSWER, STRUCT, "weivel_standard_scope", NULL, "release_scope"), REFUSAL_PARAMETER};
static const expected_parameter SCOPES_RELEASE_SCOPE[] = {
    CONTEXT_PARAMETER, PARAMETER("scope", POINTER, ANSWER, STRUCT, "weivel_standard_scope", NULL, NULL)};
static const expected_operation SCOPES_OPERATIONS[] = {
    OPERATION(weivel_standard_scopes, open, STATUS, SCOPES_OPEN),
    OPERATION(weivel_standard_scopes, release_scope, NOTHING, SCOPES_RELEASE_SCOPE),
    OPERATION(weivel_standard_scopes, release_refusal, NOTHING, RELEASE_REFUSAL)};
static const expected_description SCOPES = {"weivel_standard_scopes", SCOPES_TYPES, COUNT(SCOPES_TYPES), NULL, 0,
                                            SCOPES_OPERATIONS, COUNT(SCOPES_OPERATIONS), NULL, 0};

static const expected_member HOST_MEMBERS[] = {MEMBER(weivel_standard_host, size, U32, NONE, NULL, NULL),
                                               MEMBER(weivel_standard_host, context, CONTEXT, NONE, NULL, NULL)};
static const expected_member PLAN_MEMBERS[] = {
    MEMBER(weivel_standard_host_plan, size, U32, NONE, NULL, NULL),
    MEMBER(weivel_standard_host_plan, mode, U32, NONE, "weivel_standard_host_mode", NULL),
    MEMBER(weivel_standard_host_plan, libraries, ARRAY, STRUCT, "weivel_standard_host_library", "library_count"),
    MEMBER(weivel_standard_host_plan, library_count, SIZE, NONE, NULL, NULL),
    MEMBER(weivel_standard_host_plan, plugins, ARRAY, STRUCT, "weivel_standard_host_plugin", "plugin_count"),
    MEMBER(weivel_standard_host_plan, plugin_count, SIZE, NONE, NULL, NULL),
    MEMBER(weivel_standard_host_plan, action_plugin, TEXT, NONE, NULL, NULL),
    MEMBER(weivel_standard_host_plan, action_name, TEXT, NONE, NULL, NULL),
    MEMBER(weivel_standard_host_plan, action_arguments, VALUE, NONE, NULL, NULL),
    MEMBER(weivel_standard_host_plan, storage, STORAGE, NONE, NULL, NULL)};
static const expected_member LIBRARY_MEMBERS[] = {
    MEMBER(weivel_standard_host_library, binary, TEXT, NONE, NULL, NULL),
    MEMBER(weivel_standard_host_library, declaration, TEXT, NONE, NULL, NULL)};
static const expected_member PLUGIN_MEMBERS[] = {MEMBER(weivel_standard_host_plugin, directory, TEXT, NONE, NULL, NULL),
                                                 MEMBER(weivel_standard_host_plugin, manifest, TEXT, NONE, NULL, NULL),
                                                 MEMBER(weivel_standard_host_plugin, entry, TEXT, NONE, NULL, NULL)};
static const expected_member OUTCOME_MEMBERS[] = {
    MEMBER(weivel_standard_host_outcome, manifest, TEXT, NONE, NULL, NULL),
    MEMBER(weivel_standard_host_outcome, state, U32, NONE, "weivel_standard_host_plugin_state", NULL),
    MEMBER(weivel_standard_host_outcome, detail, TEXT, NONE, NULL, NULL)};
static const expected_member NEXT_MEMBERS[] = {MEMBER(weivel_standard_host_next, size, U32, NONE, NULL, NULL),
                                               MEMBER(weivel_standard_host_next, has_next, U32, NONE, NULL, NULL),
                                               MEMBER(weivel_standard_host_next, next, I64, NONE, NULL, NULL),
                                               MEMBER(weivel_standard_host_next, storage, STORAGE, NONE, NULL, NULL)};
static const expected_type HOST_TYPES[] = {
    TYPE(weivel_standard_host, FIELD_END(weivel_standard_host, release_refusal), HOST_MEMBERS),
    REFUSAL_TYPE,
    TYPE(weivel_standard_host_plan, FIELD_END(weivel_standard_host_plan, storage), PLAN_MEMBERS),
    TYPE(weivel_standard_host_library, sizeof(weivel_standard_host_library), LIBRARY_MEMBERS),
    TYPE(weivel_standard_host_plugin, sizeof(weivel_standard_host_plugin), PLUGIN_MEMBERS),
    TYPE(weivel_standard_host_outcome, sizeof(weivel_standard_host_outcome), OUTCOME_MEMBERS),
    TYPE(weivel_standard_host_next, FIELD_END(weivel_standard_host_next, storage), NEXT_MEMBERS)};
static const weivel_standard_described_enum_entry HOST_MODES[] = {{"WEIVEL_STANDARD_HOST_SERVE", 1},
                                                                  {"WEIVEL_STANDARD_HOST_ACTION", 2}};
static const weivel_standard_described_enum_entry PLUGIN_STATES[] = {{"WEIVEL_STANDARD_HOST_ACTIVATED", 1},
                                                                     {"WEIVEL_STANDARD_HOST_REFUSED", 2},
                                                                     {"WEIVEL_STANDARD_HOST_FAILED", 3}};
static const expected_enum HOST_ENUMS[] = {
    {"weivel_standard_host_mode", HOST_MODES, COUNT(HOST_MODES)},
    {"weivel_standard_host_plugin_state", PLUGIN_STATES, COUNT(PLUGIN_STATES)}};
static const expected_parameter HOST_START[] = {
    CONTEXT_PARAMETER, PARAMETER("arguments", TEXTS, INPUT, NONE, NULL, "argument_count", NULL),
    PARAMETER("argument_count", SIZE, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("plan", POINTER, ANSWER, STRUCT, "weivel_standard_host_plan", NULL, "release_plan"), REFUSAL_PARAMETER};
static const expected_parameter HOST_STARTED[] = {
    CONTEXT_PARAMETER,
    PARAMETER("outcomes", ARRAY, INPUT, STRUCT, "weivel_standard_host_outcome", "outcome_count", NULL),
    PARAMETER("outcome_count", SIZE, INPUT, NONE, NULL, NULL, NULL), REFUSAL_PARAMETER};
static const expected_parameter HOST_TICK[] = {
    CONTEXT_PARAMETER, PARAMETER("now", I64, INPUT, NONE, NULL, NULL, NULL),
    PARAMETER("next", POINTER, ANSWER, STRUCT, "weivel_standard_host_next", NULL, "release_next"), REFUSAL_PARAMETER};
static const expected_parameter HOST_FINISHED[] = {CONTEXT_PARAMETER,
                                                   PARAMETER("result", POINTER, INPUT, VALUE, NULL, NULL, NULL),
                                                   PARAMETER("failure", TEXT, INPUT, NONE, NULL, NULL, NULL),
                                                   REFUSAL_PARAMETER};
static const expected_parameter HOST_STOP[] = {CONTEXT_PARAMETER,
                                               PARAMETER("exit_code", U32, ANSWER, NONE, NULL, NULL, NULL),
                                               REFUSAL_PARAMETER};
static const expected_parameter HOST_RELEASE_PLAN[] = {
    CONTEXT_PARAMETER, PARAMETER("plan", POINTER, ANSWER, STRUCT, "weivel_standard_host_plan", NULL, NULL)};
static const expected_parameter HOST_RELEASE_NEXT[] = {
    CONTEXT_PARAMETER, PARAMETER("next", POINTER, ANSWER, STRUCT, "weivel_standard_host_next", NULL, NULL)};
static const expected_operation HOST_OPERATIONS[] = {
    OPERATION(weivel_standard_host, start, STATUS, HOST_START),
    OPERATION(weivel_standard_host, started, STATUS, HOST_STARTED),
    OPERATION(weivel_standard_host, tick, STATUS, HOST_TICK),
    OPERATION(weivel_standard_host, finished, STATUS, HOST_FINISHED),
    OPERATION(weivel_standard_host, stop, STATUS, HOST_STOP),
    OPERATION(weivel_standard_host, release_plan, NOTHING, HOST_RELEASE_PLAN),
    OPERATION(weivel_standard_host, release_next, NOTHING, HOST_RELEASE_NEXT),
    OPERATION(weivel_standard_host, release_refusal, NOTHING, RELEASE_REFUSAL)};
static const char *const HOST_BLOCKING[] = {"tick"};
static const expected_description HOST = {"weivel_standard_host", HOST_TYPES,      COUNT(HOST_TYPES),
                                          HOST_ENUMS,             COUNT(HOST_ENUMS), HOST_OPERATIONS,
                                          COUNT(HOST_OPERATIONS), HOST_BLOCKING,   COUNT(HOST_BLOCKING)};

static void describes_without_nesting(const weivel_standard_description *description,
                                      const expected_description *expected) {
    if (walk(description, expected)) {
        CHECK(description->definition_count == 0);
    }
}

/** The scopes' description nests the four services by the very descriptions the standard exports for them. */
static void describes_scopes(void) {
    const weivel_standard_description *scopes = weivel_standard_scopes_description();
    const weivel_standard_description *const services[] = {
        weivel_standard_config_description(), weivel_standard_secrets_description(),
        weivel_standard_paths_description(), weivel_standard_log_description()};
    size_t at;
    if (!walk(scopes, &SCOPES)) {
        return;
    }
    CHECK(scopes->definition_count == COUNT(services));
    for (at = 0; at < scopes->definition_count && at < COUNT(services); at++) {
        if (scopes->definitions[at] != services[at]) {
            fprintf(stderr, "scopes: definitions[%u] is not the exported description of %s\n", (unsigned)at,
                    shown(services[at]->definition));
            check_failures++;
        }
    }
    CHECK(weivel_standard_scopes_description() == scopes);
}

int main(void) {
    describes_without_nesting(weivel_standard_config_description(), &CONFIG);
    describes_without_nesting(weivel_standard_secrets_description(), &SECRETS);
    describes_without_nesting(weivel_standard_paths_description(), &PATHS);
    describes_without_nesting(weivel_standard_log_description(), &LOG);
    describes_scopes();
    describes_without_nesting(weivel_standard_host_description(), &HOST);
    return check_summary("describe_services_test");
}
