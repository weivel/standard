#include "writer.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "allocation.h"
#include "tokens.h"

#if defined(__GNUC__) || defined(__clang__)
#define DESCRIBE_PRINTF(format_at, arguments_at) __attribute__((format(printf, format_at, arguments_at)))
#else
#define DESCRIBE_PRINTF(format_at, arguments_at)
#endif

/** The source being written; `failed` once memory ran out, after which nothing more is written. */
typedef struct writing {
    describe_text *source;
    const describe_service *service;
    int failed;
} writing;

static const char *const KIND_NAMES[] = {
    "WEIVEL_STANDARD_DESCRIBED_NONE",   "WEIVEL_STANDARD_DESCRIBED_U32",     "WEIVEL_STANDARD_DESCRIBED_U64",
    "WEIVEL_STANDARD_DESCRIBED_I64",    "WEIVEL_STANDARD_DESCRIBED_F64",     "WEIVEL_STANDARD_DESCRIBED_SIZE",
    "WEIVEL_STANDARD_DESCRIBED_TEXT",   "WEIVEL_STANDARD_DESCRIBED_TEXTS",   "WEIVEL_STANDARD_DESCRIBED_BYTES",
    "WEIVEL_STANDARD_DESCRIBED_STRUCT", "WEIVEL_STANDARD_DESCRIBED_POINTER", "WEIVEL_STANDARD_DESCRIBED_ARRAY",
    "WEIVEL_STANDARD_DESCRIBED_VALUE",  "WEIVEL_STANDARD_DESCRIBED_HANDLE",  "WEIVEL_STANDARD_DESCRIBED_CONTEXT",
    "WEIVEL_STANDARD_DESCRIBED_STORAGE"};

static const char *const ROLE_NAMES[] = {"WEIVEL_STANDARD_DESCRIBED_NO_ROLE", "WEIVEL_STANDARD_DESCRIBED_INPUT",
                                         "WEIVEL_STANDARD_DESCRIBED_ANSWER", "WEIVEL_STANDARD_DESCRIBED_REFUSAL"};

static const char *const RETURN_NAMES[] = {"WEIVEL_STANDARD_DESCRIBED_NO_RETURN", "WEIVEL_STANDARD_DESCRIBED_STATUS",
                                           "WEIVEL_STANDARD_DESCRIBED_NOTHING"};

#define NAME_OF(names, number) \
    ((number) < sizeof(names) / sizeof((names)[0]) ? (names)[number] : "the unknown number")

static int grow(describe_text *text, size_t needed) {
    size_t capacity = text->capacity == 0 ? 4096 : text->capacity;
    char *grown;
    while (capacity < needed) {
        if (capacity > ((size_t)-1) / 2) {
            return 0;
        }
        capacity *= 2;
    }
    grown = weivel_standard_memory_reallocate(text->bytes, capacity);
    if (grown == NULL) {
        return 0;
    }
    text->bytes = grown;
    text->capacity = capacity;
    return 1;
}

static void put(writing *state, const char *format, ...) DESCRIBE_PRINTF(2, 3);

static void put(writing *state, const char *format, ...) {
    describe_text *text = state->source;
    va_list arguments;
    int length;
    if (state->failed) {
        return;
    }
    va_start(arguments, format);
    length = vsnprintf(NULL, 0, format, arguments);
    va_end(arguments);
    if (length < 0 || (text->length + (size_t)length + 1 > text->capacity &&
                       !grow(text, text->length + (size_t)length + 1))) {
        state->failed = 1;
        return;
    }
    va_start(arguments, format);
    vsnprintf(text->bytes + text->length, (size_t)length + 1, format, arguments);
    va_end(arguments);
    text->length += (size_t)length;
}

static void put_name(writing *state, const char *field, const char *name, const char *after) {
    if (name == NULL) {
        put(state, "%s = NULL%s", field, after);
    } else {
        put(state, "%s = \"%s\"%s", field, name, after);
    }
}

static void put_count(writing *state, const char *field, const char *array, const char *suffix) {
    const char *prefix = state->service->prefix;
    put(state, "     .%s = sizeof(%s%s%s) / sizeof(%s%s%s[0])", field, prefix, array, suffix, prefix, array, suffix);
}

static void put_member(writing *state, const describe_type *type, const describe_item *member, int last) {
    put_name(state, "    {.name", member->name, ",\n");
    put(state, "     .offset = offsetof(%s, %s),\n", type->name, member->name);
    put(state, "     .size = sizeof(((%s *)0)->%s),\n", type->name, member->name);
    put(state, "     .kind = %s,\n", NAME_OF(KIND_NAMES, member->kind));
    put(state, "     .element = %s,\n", NAME_OF(KIND_NAMES, member->element));
    put_name(state, "     .type", member->type, ",\n");
    put_name(state, "     .count", member->count, ",\n");
    put_name(state, "     .release", member->release, last ? "}};\n\n" : "},\n");
}

static void put_members(writing *state, const describe_type *type) {
    size_t at;
    put(state, "static const weivel_standard_described_member %s%s_members[] = {\n", state->service->prefix,
        type->name);
    for (at = 0; at < type->member_count; at++) {
        put_member(state, type, &type->members[at], at + 1 == type->member_count);
    }
}

static void put_entries(writing *state, const describe_enum *described) {
    size_t at;
    put(state, "static const weivel_standard_described_enum_entry %s%s_entries[] = {\n", state->service->prefix,
        described->name);
    for (at = 0; at < described->entry_count; at++) {
        const char *name = described->entries[at].name;
        const char *after = at + 1 == described->entry_count ? "};" : ",";
        put(state, "    {.name = \"%s\", .number = %s}%s\n", name, name, after);
    }
    put(state, "\n");
}

static void put_parameter(writing *state, const describe_item *parameter, int last) {
    put_name(state, "    {.name", parameter->name, ",\n");
    put(state, "     .kind = %s,\n", NAME_OF(KIND_NAMES, parameter->kind));
    put(state, "     .role = %s,\n", NAME_OF(ROLE_NAMES, parameter->role));
    put(state, "     .element = %s,\n", NAME_OF(KIND_NAMES, parameter->element));
    put_name(state, "     .type", parameter->type, ",\n");
    put_name(state, "     .count", parameter->count, ",\n");
    put_name(state, "     .release", parameter->release, last ? "}};\n\n" : "},\n");
}

static void put_parameters(writing *state, const describe_operation *operation) {
    size_t at;
    put(state, "static const weivel_standard_described_parameter %s%s_parameters[] = {\n", state->service->prefix,
        operation->name);
    for (at = 0; at < operation->parameter_count; at++) {
        put_parameter(state, &operation->parameters[at], at + 1 == operation->parameter_count);
    }
}

static void put_first_shape(writing *state, const describe_type *type) {
    if (type->sized) {
        put(state, "     .first_shape = offsetof(%s, %s) + sizeof(((%s *)0)->%s),\n", type->name, type->first_shape,
            type->name, type->first_shape);
    } else {
        put(state, "     .first_shape = sizeof(%s),\n", type->name);
    }
}

static void put_types(writing *state, const describe_reach *reach) {
    size_t at;
    put(state, "static const weivel_standard_described_type %stypes[] = {\n", state->service->prefix);
    for (at = 0; at < reach->type_count; at++) {
        const describe_type *type = reach->types[at];
        put_name(state, "    {.name", type->name, ",\n");
        put(state, "     .size = sizeof(%s),\n", type->name);
        put_first_shape(state, type);
        put(state, "     .members = %s%s_members,\n", state->service->prefix, type->name);
        put_count(state, "member_count", type->name, "_members");
        put(state, at + 1 == reach->type_count ? "}};\n\n" : "},\n");
    }
}

static void put_enums(writing *state, const describe_reach *reach) {
    size_t at;
    if (reach->enum_count == 0) {
        return;
    }
    put(state, "static const weivel_standard_described_enum %senums[] = {\n", state->service->prefix);
    for (at = 0; at < reach->enum_count; at++) {
        const describe_enum *described = reach->enums[at];
        put_name(state, "    {.name", described->name, ",\n");
        put(state, "     .entries = %s%s_entries,\n", state->service->prefix, described->name);
        put_count(state, "entry_count", described->name, "_entries");
        put(state, at + 1 == reach->enum_count ? "}};\n\n" : "},\n");
    }
}

static void put_operations(writing *state, const describe_type *definition) {
    size_t at;
    put(state, "static const weivel_standard_described_operation %soperations[] = {\n", state->service->prefix);
    for (at = 0; at < definition->operation_count; at++) {
        const describe_operation *operation = &definition->operations[at];
        put_name(state, "    {.name", operation->name, ",\n");
        put(state, "     .offset = offsetof(%s, %s),\n", definition->name, operation->name);
        put(state, "     .returns = %s,\n", NAME_OF(RETURN_NAMES, operation->returns));
        put(state, "     .parameters = %s%s_parameters,\n", state->service->prefix, operation->name);
        put_count(state, "parameter_count", operation->name, "_parameters");
        put(state, at + 1 == definition->operation_count ? "}};\n\n" : "},\n");
    }
}

static void put_description(writing *state, const describe_reach *reach) {
    const char *prefix = state->service->prefix;
    put(state, "static const weivel_standard_description %sdescription = {\n", prefix);
    put(state, "    .size = (uint32_t)sizeof(weivel_standard_description),\n");
    put(state, "    .definition = \"%s\",\n", reach->definition->name);
    put(state, "    .types = %stypes,\n", prefix);
    put(state, "    .type_count = sizeof(%stypes) / sizeof(%stypes[0]),\n", prefix, prefix);
    if (reach->enum_count == 0) {
        put(state, "    .enums = NULL,\n    .enum_count = 0,\n");
    } else {
        put(state, "    .enums = %senums,\n", prefix);
        put(state, "    .enum_count = sizeof(%senums) / sizeof(%senums[0]),\n", prefix, prefix);
    }
    put(state, "    .operations = %soperations,\n", prefix);
    put(state, "    .operation_count = sizeof(%soperations) / sizeof(%soperations[0])};\n\n", prefix, prefix);
}

static void put_symbol(writing *state) {
    const describe_service *service = state->service;
    const char *macro = service->export_macro == NULL ? "" : service->export_macro;
    const char *space = service->export_macro == NULL ? "" : " ";
    put(state, "%s%sconst weivel_standard_description *%s(void);\n\n", macro, space, service->symbol);
    put(state, "%s%sconst weivel_standard_description *%s(void) {\n", macro, space, service->symbol);
    put(state, "    return &%sdescription;\n}\n", service->prefix);
}

static void put_head(writing *state) {
    size_t at;
    put(state, "/* Written by weivel-describe from the headers it includes; edit them, not this file. */\n\n");
    put(state, "#include <stddef.h>\n#include <stdint.h>\n\n#include \"weivel/description.h\"\n");
    for (at = 0; at < state->service->include_count; at++) {
        put(state, "#include \"%s\"\n", state->service->includes[at]);
    }
    put(state, "\n");
}

static void put_source(writing *state, const describe_reach *reach) {
    size_t at;
    put_head(state);
    for (at = 0; at < reach->type_count; at++) {
        put_members(state, reach->types[at]);
    }
    for (at = 0; at < reach->enum_count; at++) {
        put_entries(state, reach->enums[at]);
    }
    for (at = 0; at < reach->definition->operation_count; at++) {
        put_parameters(state, &reach->definition->operations[at]);
    }
    put_types(state, reach);
    put_enums(state, reach);
    put_operations(state, reach->definition);
    put_description(state, reach);
    put_symbol(state);
}

static int is_identifier(const char *text) {
    size_t at;
    if (text == NULL || text[0] == '\0' || (text[0] >= '0' && text[0] <= '9')) {
        return 0;
    }
    for (at = 0; text[at] != '\0'; at++) {
        char character = text[at];
        if (!((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
              (character >= '0' && character <= '9') || character == '_')) {
            return 0;
        }
    }
    return 1;
}

static int is_include(const char *text) {
    return text != NULL && text[0] != '\0' && strpbrk(text, "\"\\\r\n") == NULL;
}

static describe_outcome check_identifier(const char *text, const char *what, describe_refusal *refusal) {
    return is_identifier(text) ? DESCRIBE_DONE
                               : describe_refuse(refusal, NULL, 0, "the %s `%s` is not a C identifier", what,
                                                 text == NULL ? "" : text);
}

static describe_outcome check_service(const describe_model *model, const describe_service *service,
                                      describe_refusal *refusal) {
    describe_outcome outcome = check_identifier(service->symbol, "symbol", refusal);
    size_t at;
    if (outcome == DESCRIBE_DONE) {
        outcome = check_identifier(service->prefix, "prefix", refusal);
    }
    if (outcome == DESCRIBE_DONE && service->export_macro != NULL) {
        outcome = check_identifier(service->export_macro, "export macro", refusal);
    }
    if (outcome == DESCRIBE_DONE && service->include_count != model->file_count) {
        return describe_refuse(refusal, NULL, 0, "%u includes were named for %u headers read",
                               (unsigned)service->include_count, (unsigned)model->file_count);
    }
    for (at = 0; outcome == DESCRIBE_DONE && at < service->include_count; at++) {
        if (!is_include(service->includes[at])) {
            outcome = describe_refuse(refusal, NULL, 0,
                                      "the include `%s` is empty or holds a quote, a backslash or a line end",
                                      service->includes[at] == NULL ? "" : service->includes[at]);
        }
    }
    return outcome;
}

describe_outcome describe_write(const describe_model *model, const describe_service *service, describe_text *source,
                                describe_refusal *refusal) {
    describe_reach reach;
    writing state;
    describe_outcome outcome;
    memset(source, 0, sizeof *source);
    memset(refusal, 0, sizeof *refusal);
    memset(&reach, 0, sizeof reach);
    outcome = check_service(model, service, refusal);
    if (outcome != DESCRIBE_DONE) {
        return outcome;
    }
    outcome = describe_reach_of(model, service->definition, &reach, refusal);
    if (outcome != DESCRIBE_DONE) {
        return outcome;
    }
    state.source = source;
    state.service = service;
    state.failed = 0;
    put_source(&state, &reach);
    describe_reach_release(&reach);
    if (state.failed) {
        describe_text_release(source);
        return DESCRIBE_OUT_OF_MEMORY;
    }
    return DESCRIBE_DONE;
}

void describe_text_release(describe_text *text) {
    weivel_standard_memory_release(text->bytes);
    memset(text, 0, sizeof *text);
}
