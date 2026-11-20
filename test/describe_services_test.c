#include <stdio.h>
#include <string.h>

#include "check.h"
#include "weivel/description.h"
#include "weivel/services.h"

#define K(name) WEIVEL_STANDARD_DESCRIBED_##name
#define FIELD_END(type, field) (offsetof(type, field) + sizeof(((type *)0)->field))

const weivel_standard_description *services_described_config(void);
const weivel_standard_description *services_described_secrets(void);
const weivel_standard_description *services_described_paths(void);

static const weivel_standard_described_type *type_named(const weivel_standard_description *description,
                                                        const char *name) {
    size_t at;
    for (at = 0; at < description->type_count; at++) {
        if (strcmp(description->types[at].name, name) == 0) {
            return &description->types[at];
        }
    }
    fprintf(stderr, "%s: no type %s\n", description->definition, name);
    check_failures++;
    return NULL;
}

static void check_type(const weivel_standard_description *description, const char *name, size_t size,
                       size_t first_shape, size_t member_count) {
    const weivel_standard_described_type *type = type_named(description, name);
    if (type != NULL) {
        CHECK(type->size == size);
        CHECK(type->first_shape == first_shape);
        CHECK(type->member_count == member_count);
    }
}

static void check_member(const weivel_standard_description *description, const char *type_name, const char *name,
                         size_t offset, size_t size, uint32_t kind) {
    const weivel_standard_described_type *type = type_named(description, type_name);
    size_t at;
    if (type == NULL) {
        return;
    }
    for (at = 0; at < type->member_count; at++) {
        const weivel_standard_described_member *member = &type->members[at];
        if (strcmp(member->name, name) == 0) {
            if (member->offset != offset || member->size != size || member->kind != kind) {
                fprintf(stderr, "%s.%s: expected offset %u, size %u, kind %u; got %u, %u, %u\n", type_name, name,
                        (unsigned)offset, (unsigned)size, (unsigned)kind, (unsigned)member->offset,
                        (unsigned)member->size, (unsigned)member->kind);
                check_failures++;
            }
            return;
        }
    }
    fprintf(stderr, "%s: no member %s\n", type_name, name);
    check_failures++;
}

static void check_operation(const weivel_standard_description *description, size_t at, const char *name,
                            size_t offset, uint32_t returns, size_t parameter_count) {
    const weivel_standard_described_operation *operation;
    if (at >= description->operation_count) {
        fprintf(stderr, "%s: no operation %u\n", description->definition, (unsigned)at);
        check_failures++;
        return;
    }
    operation = &description->operations[at];
    check_text(operation->name, name, "operation name");
    if (operation->offset != offset || operation->returns != returns ||
        operation->parameter_count != parameter_count) {
        fprintf(stderr, "%s.%s: expected offset %u, returns %u, %u parameters; got %u, %u, %u\n",
                description->definition, name, (unsigned)offset, (unsigned)returns, (unsigned)parameter_count,
                (unsigned)operation->offset, (unsigned)operation->returns, (unsigned)operation->parameter_count);
        check_failures++;
    }
}

static int passes_the_check(const weivel_standard_description *description) {
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    weivel_status status;
    if (description == NULL) {
        fprintf(stderr, "a symbol answered no description\n");
        check_failures++;
        return 0;
    }
    status = weivel_standard_description_check(description, &refusal);
    if (status != WEIVEL_STATUS_OK) {
        fprintf(stderr, "%s: refused by the check at %s: %s\n", description->definition,
                refusal.path == NULL ? "(none)" : refusal.path, refusal.detail == NULL ? "(none)" : refusal.detail);
        weivel_standard_refusal_release(&refusal);
        check_failures++;
        return 0;
    }
    CHECK(description->size == sizeof(weivel_standard_description));
    return 1;
}

static void describes_config(void) {
    const weivel_standard_description *config = services_described_config();
    const weivel_standard_described_parameter *answer;
    if (!passes_the_check(config)) {
        return;
    }
    check_text(config->definition, "weivel_standard_config", "config definition");
    CHECK(config->type_count == 6);
    check_type(config, "weivel_standard_config", sizeof(weivel_standard_config),
               FIELD_END(weivel_standard_config, release_refusal), 2);
    check_type(config, "weivel_standard_service_refusal", sizeof(weivel_standard_service_refusal),
               FIELD_END(weivel_standard_service_refusal, storage), 4);
    check_type(config, "weivel_standard_setting", sizeof(weivel_standard_setting), sizeof(weivel_standard_setting),
               2);
    check_member(config, "weivel_standard_settings", "count", offsetof(weivel_standard_settings, count),
                 sizeof(size_t), K(SIZE));
    check_member(config, "weivel_standard_setting_value", "number", offsetof(weivel_standard_setting_value, number),
                 sizeof(double), K(F64));
    check_member(config, "weivel_standard_setting_value", "text_length",
                 offsetof(weivel_standard_setting_value, text_length), sizeof(size_t), K(SIZE));
    check_member(config, "weivel_standard_setting", "value", offsetof(weivel_standard_setting, value),
                 sizeof(weivel_standard_setting_value), K(STRUCT));
    check_member(config, "weivel_standard_setting_answer", "storage",
                 offsetof(weivel_standard_setting_answer, storage), sizeof(void *), K(STORAGE));
    check_operation(config, 0, "all", offsetof(weivel_standard_config, all), K(STATUS), 3);
    check_operation(config, 1, "get", offsetof(weivel_standard_config, get), K(STATUS), 4);
    check_operation(config, 2, "set", offsetof(weivel_standard_config, set), K(STATUS), 4);
    check_operation(config, 3, "release_settings", offsetof(weivel_standard_config, release_settings), K(NOTHING),
                    2);
    check_operation(config, 4, "release_answer", offsetof(weivel_standard_config, release_answer), K(NOTHING), 2);
    check_operation(config, 5, "release_refusal", offsetof(weivel_standard_config, release_refusal), K(NOTHING), 2);
    CHECK(config->operation_count == 6);
    if (config->operation_count > 1) {
        answer = &config->operations[1].parameters[2];
        check_text(answer->name, "answer", "get's answer");
        CHECK(answer->role == K(ANSWER) && answer->kind == K(POINTER) && answer->element == K(STRUCT));
        check_text(answer->release, "release_answer", "get's answer release");
    }
    CHECK(config->enum_count == 1);
    if (config->enum_count == 1) {
        check_text(config->enums[0].name, "weivel_standard_setting_kind", "config enum");
        CHECK(config->enums[0].entry_count == 4 &&
              config->enums[0].entries[3].number == WEIVEL_STANDARD_SETTING_BOOLEAN);
    }
}

static void describes_secrets(void) {
    const weivel_standard_description *secrets = services_described_secrets();
    if (!passes_the_check(secrets)) {
        return;
    }
    check_text(secrets->definition, "weivel_standard_secrets", "secrets definition");
    CHECK(secrets->type_count == 3 && secrets->enum_count == 0 && secrets->enums == NULL);
    check_type(secrets, "weivel_standard_secret", sizeof(weivel_standard_secret),
               FIELD_END(weivel_standard_secret, storage), 4);
    check_member(secrets, "weivel_standard_secret", "bytes", offsetof(weivel_standard_secret, bytes),
                 sizeof(const uint8_t *), K(BYTES));
    check_member(secrets, "weivel_standard_secret", "length", offsetof(weivel_standard_secret, length),
                 sizeof(size_t), K(SIZE));
    check_operation(secrets, 0, "remove", offsetof(weivel_standard_secrets, remove), K(STATUS), 3);
    check_operation(secrets, 2, "set", offsetof(weivel_standard_secrets, set), K(STATUS), 5);
    check_operation(secrets, 4, "release_refusal", offsetof(weivel_standard_secrets, release_refusal), K(NOTHING),
                    2);
    CHECK(secrets->operation_count == 5);
}

static void describes_paths(void) {
    const weivel_standard_description *paths = services_described_paths();
    if (!passes_the_check(paths)) {
        return;
    }
    check_text(paths->definition, "weivel_standard_paths", "paths definition");
    CHECK(paths->type_count == 1);
    check_type(paths, "weivel_standard_paths", sizeof(weivel_standard_paths), FIELD_END(weivel_standard_paths, plugin),
               2);
    check_member(paths, "weivel_standard_paths", "context", offsetof(weivel_standard_paths, context), sizeof(void *),
                 K(CONTEXT));
    check_operation(paths, 0, "cache", offsetof(weivel_standard_paths, cache), K(STATUS), 2);
    check_operation(paths, 1, "config", offsetof(weivel_standard_paths, config), K(STATUS), 2);
    check_operation(paths, 2, "plugin", offsetof(weivel_standard_paths, plugin), K(STATUS), 2);
    CHECK(paths->operation_count == 3);
    if (paths->operation_count == 3) {
        const weivel_standard_described_parameter *directory = &paths->operations[2].parameters[1];
        check_text(directory->name, "directory", "plugin's directory");
        CHECK(directory->kind == K(TEXT) && directory->role == K(ANSWER) && directory->release == NULL);
    }
}

int main(void) {
    describes_config();
    describes_secrets();
    describes_paths();
    return check_summary("describe_services_test");
}
