#include "writer.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "allocation.h"
#include "reading.h"
#include "tokens.h"

#if defined(__GNUC__) || defined(__clang__)
#define DESCRIBE_PRINTF(format_at, arguments_at) __attribute__((format(printf, format_at, arguments_at)))
#else
#define DESCRIBE_PRINTF(format_at, arguments_at)
#endif

/** One slot of a `definitions` array the symbol fills on its first call: `<prefix>definitions[at]`. */
typedef struct filled_slot {
    char *prefix;
    size_t at;
    const char *definition;
} filled_slot;

/**
 * The source being written; `failed` once memory ran out, after which nothing more is written. `prefix` begins every
 * name of the description being written, the service's own or a nested definition's. `written` names each nested
 * definition already written and `open` each one being written, so a definition is written once and one reaching
 * itself is refused. `answered_by` is the function the description being written names, the service's for its own and
 * NULL for a nested one, which no symbol answers.
 */
typedef struct writing {
    describe_text *source;
    const describe_service *service;
    const describe_model *model;
    describe_refusal *refusal;
    int failed;
    const char *prefix;
    const char *answered_by;
    const char **written;
    size_t written_count;
    const char **open;
    size_t open_count;
    filled_slot *filled;
    size_t filled_count;
} writing;

static const char *const KIND_NAMES[] = {
    "WEIVEL_STANDARD_DESCRIBED_NONE",   "WEIVEL_STANDARD_DESCRIBED_U32",     "WEIVEL_STANDARD_DESCRIBED_U64",
    "WEIVEL_STANDARD_DESCRIBED_I64",    "WEIVEL_STANDARD_DESCRIBED_F64",     "WEIVEL_STANDARD_DESCRIBED_SIZE",
    "WEIVEL_STANDARD_DESCRIBED_TEXT",   "WEIVEL_STANDARD_DESCRIBED_TEXTS",   "WEIVEL_STANDARD_DESCRIBED_BYTES",
    "WEIVEL_STANDARD_DESCRIBED_STRUCT", "WEIVEL_STANDARD_DESCRIBED_POINTER", "WEIVEL_STANDARD_DESCRIBED_ARRAY",
    "WEIVEL_STANDARD_DESCRIBED_VALUE",  "WEIVEL_STANDARD_DESCRIBED_HANDLE",  "WEIVEL_STANDARD_DESCRIBED_CONTEXT",
    "WEIVEL_STANDARD_DESCRIBED_STORAGE", "WEIVEL_STANDARD_DESCRIBED_DEFINITION"};

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
    const char *prefix = state->prefix;
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
    put(state, "static const weivel_standard_described_member %s%s_members[] = {\n", state->prefix,
        type->name);
    for (at = 0; at < type->member_count; at++) {
        put_member(state, type, &type->members[at], at + 1 == type->member_count);
    }
}

static void put_entries(writing *state, const describe_enum *described) {
    size_t at;
    put(state, "static const weivel_standard_described_enum_entry %s%s_entries[] = {\n", state->prefix,
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
    put(state, "static const weivel_standard_described_parameter %s%s_parameters[] = {\n", state->prefix,
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
    put(state, "static const weivel_standard_described_type %stypes[] = {\n", state->prefix);
    for (at = 0; at < reach->type_count; at++) {
        const describe_type *type = reach->types[at];
        put_name(state, "    {.name", type->name, ",\n");
        put(state, "     .size = sizeof(%s),\n", type->name);
        put_first_shape(state, type);
        put(state, "     .members = %s%s_members,\n", state->prefix, type->name);
        put_count(state, "member_count", type->name, "_members");
        put(state, at + 1 == reach->type_count ? "}};\n\n" : "},\n");
    }
}

static void put_enums(writing *state, const describe_reach *reach) {
    size_t at;
    if (reach->enum_count == 0) {
        return;
    }
    put(state, "static const weivel_standard_described_enum %senums[] = {\n", state->prefix);
    for (at = 0; at < reach->enum_count; at++) {
        const describe_enum *described = reach->enums[at];
        put_name(state, "    {.name", described->name, ",\n");
        put(state, "     .entries = %s%s_entries,\n", state->prefix, described->name);
        put_count(state, "entry_count", described->name, "_entries");
        put(state, at + 1 == reach->enum_count ? "}};\n\n" : "},\n");
    }
}

static void put_operations(writing *state, const describe_type *definition) {
    size_t at;
    put(state, "static const weivel_standard_described_operation %soperations[] = {\n", state->prefix);
    for (at = 0; at < definition->operation_count; at++) {
        const describe_operation *operation = &definition->operations[at];
        put_name(state, "    {.name", operation->name, ",\n");
        put(state, "     .offset = offsetof(%s, %s),\n", definition->name, operation->name);
        put(state, "     .returns = %s,\n", NAME_OF(RETURN_NAMES, operation->returns));
        put(state, "     .parameters = %s%s_parameters,\n", state->prefix, operation->name);
        put_count(state, "parameter_count", operation->name, "_parameters");
        put(state, at + 1 == definition->operation_count ? "}};\n\n" : "},\n");
    }
}

static int has_blocking(const describe_type *definition) {
    size_t at;
    for (at = 0; at < definition->operation_count; at++) {
        if (definition->operations[at].blocking) {
            return 1;
        }
    }
    return 0;
}

static void put_description(writing *state, const describe_reach *reach) {
    const char *prefix = state->prefix;
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
    put(state, "    .operation_count = sizeof(%soperations) / sizeof(%soperations[0]),\n", prefix, prefix);
    if (!has_blocking(reach->definition)) {
        put(state, "    .blocking = NULL,\n    .blocking_count = 0,\n");
    } else {
        put(state, "    .blocking = %sblocking,\n", prefix);
        put(state, "    .blocking_count = sizeof(%sblocking) / sizeof(%sblocking[0]),\n", prefix, prefix);
    }
    if (reach->definition_count == 0) {
        put(state, "    .definitions = NULL,\n    .definition_count = 0,\n");
    } else {
        put(state, "    .definitions = %sdefinitions,\n", prefix);
        put(state, "    .definition_count = sizeof(%sdefinitions) / sizeof(%sdefinitions[0]),\n", prefix, prefix);
    }
    put(state, "    .answered_by = %s};\n\n", state->answered_by == NULL ? "NULL" : state->answered_by);
}

static void put_signature(writing *state) {
    const describe_service *service = state->service;
    const char *macro = service->export_macro == NULL ? "" : service->export_macro;
    const char *space = service->export_macro == NULL ? "" : " ";
    put(state, "%s%sconst weivel_standard_description *%s(void)", macro, space, service->symbol);
}

static int is_declared_before(const writing *state, size_t at) {
    size_t before;
    for (before = 0; before < at; before++) {
        if (strcmp(state->filled[before].definition, state->filled[at].definition) == 0) {
            return 1;
        }
    }
    return 0;
}

/**
 * The function that fills every slot of a definition described elsewhere, run once by the platform's own once
 * primitive, since C11 has none every platform ships: `<threads.h>` is optional and absent on macOS. Each
 * `<definition>_description` is declared here unless the headers declare it, since a second declaration without
 * their export macro is inconsistent linkage to MSVC.
 */
static void put_filling(writing *state) {
    const char *prefix = state->service->prefix;
    size_t at;
    for (at = 0; at < state->filled_count; at++) {
        if (!is_declared_before(state, at) &&
            !describe_model_declares_description(state->model, state->filled[at].definition)) {
            put(state, "const weivel_standard_description *%s_description(void);\n", state->filled[at].definition);
        }
    }
    put(state, "\n#if defined(_WIN32)\n#ifndef WIN32_LEAN_AND_MEAN\n#define WIN32_LEAN_AND_MEAN\n#endif\n");
    put(state, "#include <windows.h>\n\nstatic INIT_ONCE %sfilled = INIT_ONCE_STATIC_INIT;\n\n", prefix);
    put(state, "static BOOL CALLBACK %sfill(PINIT_ONCE once, PVOID parameter, PVOID *context) {\n", prefix);
    put(state, "    (void)once;\n    (void)parameter;\n    (void)context;\n");
    for (at = 0; at < state->filled_count; at++) {
        put(state, "    %sdefinitions[%u] = %s_description();\n", state->filled[at].prefix,
            (unsigned)state->filled[at].at, state->filled[at].definition);
    }
    put(state, "    return TRUE;\n}\n#else\n#include <pthread.h>\n\n");
    put(state, "static pthread_once_t %sfilled = PTHREAD_ONCE_INIT;\n\nstatic void %sfill(void) {\n", prefix, prefix);
    for (at = 0; at < state->filled_count; at++) {
        put(state, "    %sdefinitions[%u] = %s_description();\n", state->filled[at].prefix,
            (unsigned)state->filled[at].at, state->filled[at].definition);
    }
    put(state, "}\n#endif\n\n");
}

static void put_symbol(writing *state) {
    const char *prefix = state->service->prefix;
    if (state->filled_count > 0) {
        put_filling(state);
    }
    put_signature(state);
    put(state, ";\n\n");
    put_signature(state);
    put(state, " {\n");
    if (state->filled_count > 0) {
        put(state, "#if defined(_WIN32)\n    InitOnceExecuteOnce(&%sfilled, %sfill, NULL, NULL);\n", prefix, prefix);
        put(state, "#else\n    pthread_once(&%sfilled, %sfill);\n#endif\n", prefix, prefix);
    }
    put(state, "    return &%sdescription;\n}\n", prefix);
}

/**
 * Declares the answering function unless the headers declare it, as put_filling does a description, so a misspelt
 * function is a link error rather than a NULL at run time.
 */
static void put_answering_declaration(writing *state) {
    const describe_service *service = state->service;
    const char *macro = service->export_macro == NULL ? "" : service->export_macro;
    const char *space = service->export_macro == NULL ? "" : " ";
    if (service->answered_by == NULL || describe_model_declares_function(state->model, service->answered_by)) {
        return;
    }
    put(state, "%s%sconst void *%s(void);\n\n", macro, space, service->answered_by);
}

static void put_head(writing *state) {
    size_t at;
    put(state, "/* Written by weivel-describe from the headers it includes; edit them, not this file. */\n\n");
    put(state, "#include <stddef.h>\n#include <stdint.h>\n\n#include \"weivel/description.h\"\n");
    for (at = 0; at < state->service->include_count; at++) {
        put(state, "#include \"%s\"\n", state->service->includes[at]);
    }
    put(state, "\n");
    put_answering_declaration(state);
}

static void put_blocking(writing *state, const describe_type *definition) {
    size_t at;
    const char *separator = "";
    if (!has_blocking(definition)) {
        return;
    }
    put(state, "static const char *const %sblocking[] = {", state->prefix);
    for (at = 0; at < definition->operation_count; at++) {
        if (definition->operations[at].blocking) {
            put(state, "%s\"%s\"", separator, definition->operations[at].name);
            separator = ", ";
        }
    }
    put(state, "};\n\n");
}

/** A definition the headers declare, unless it is named as described elsewhere, which wins over the scan. */
static int is_written_here(const writing *state, const char *definition) {
    const describe_type *type = describe_model_type(state->model, definition);
    return type != NULL && !type->elsewhere;
}

/** Each slot a nested description of this source, or NULL for one described elsewhere, which the symbol fills. */
static void put_definitions(writing *state, const describe_reach *reach) {
    size_t at;
    int fills = 0;
    if (reach->definition_count == 0) {
        return;
    }
    for (at = 0; at < reach->definition_count; at++) {
        fills = fills || !is_written_here(state, reach->definitions[at]);
    }
    put(state, "static const weivel_standard_description *%s%sdefinitions[] = {", fills ? "" : "const ", state->prefix);
    for (at = 0; at < reach->definition_count; at++) {
        const char *separator = at == 0 ? "" : ", ";
        if (is_written_here(state, reach->definitions[at])) {
            put(state, "%s&%s%s_description", separator, state->service->prefix, reach->definitions[at]);
        } else {
            put(state, "%sNULL", separator);
        }
    }
    put(state, "};\n\n");
}

static void put_arrays(writing *state, const describe_reach *reach) {
    size_t at;
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
    put_blocking(state, reach->definition);
    put_definitions(state, reach);
    put_description(state, reach);
}

static int names_one_of(const char *const *names, size_t count, const char *name) {
    size_t at;
    for (at = 0; at < count; at++) {
        if (strcmp(names[at], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static describe_outcome put_reached(writing *state, const char *definition);

/** Notes every slot of `definitions` that names a definition described elsewhere, for the symbol to fill. */
static describe_outcome note_filled(writing *state, const describe_reach *reach) {
    size_t at;
    for (at = 0; at < reach->definition_count; at++) {
        filled_slot *slot;
        if (is_written_here(state, reach->definitions[at])) {
            continue;
        }
        DESCRIBE_APPEND(state->filled, state->filled_count, slot);
        if (slot == NULL || (slot->prefix = describe_copy_text(state->prefix)) == NULL) {
            return DESCRIBE_OUT_OF_MEMORY;
        }
        slot->at = at;
        slot->definition = reach->definitions[at];
    }
    return DESCRIBE_DONE;
}

/** Writes every nested definition this reach names that the headers declare and no earlier part has written. */
static describe_outcome put_nested(writing *state, const describe_reach *reach) {
    describe_outcome outcome = DESCRIBE_DONE;
    size_t at;
    for (at = 0; outcome == DESCRIBE_DONE && at < reach->definition_count; at++) {
        const char *nested = reach->definitions[at];
        if (!is_written_here(state, nested) || names_one_of(state->written, state->written_count, nested)) {
            continue;
        }
        if (names_one_of(state->open, state->open_count, nested)) {
            const describe_type *type = describe_model_type(state->model, nested);
            return describe_refuse(state->refusal, type->where.file, type->where.line,
                                   "the definition `%s` reaches itself through a definition it holds or answers, "
                                   "which no description can describe",
                                   nested);
        }
        outcome = put_reached(state, nested);
    }
    return outcome;
}

/** `<service prefix><definition>_`, the prefix of a nested definition's names; NULL when memory runs out. */
static char *nested_prefix(const writing *state, const char *definition) {
    size_t prefix_length = strlen(state->service->prefix);
    size_t name_length = strlen(definition);
    char *prefix = weivel_standard_memory_allocate(prefix_length + name_length + 2);
    if (prefix != NULL) {
        memcpy(prefix, state->service->prefix, prefix_length);
        memcpy(prefix + prefix_length, definition, name_length);
        prefix[prefix_length + name_length] = '_';
        prefix[prefix_length + name_length + 1] = '\0';
    }
    return prefix;
}

/** Writes the description of `definition`, every nested one it reaches first, under the current prefix. */
static describe_outcome put_described(writing *state, const char *definition) {
    describe_reach reach;
    const char **slot;
    describe_outcome outcome = describe_reach_of(state->model, definition, &reach, state->refusal);
    if (outcome != DESCRIBE_DONE) {
        return outcome;
    }
    DESCRIBE_APPEND(state->open, state->open_count, slot);
    if (slot == NULL) {
        describe_reach_release(&reach);
        return DESCRIBE_OUT_OF_MEMORY;
    }
    *slot = reach.definition->name;
    outcome = put_nested(state, &reach);
    state->open_count--;
    if (outcome == DESCRIBE_DONE) {
        outcome = note_filled(state, &reach);
    }
    if (outcome == DESCRIBE_DONE) {
        put_arrays(state, &reach);
    }
    describe_reach_release(&reach);
    return outcome;
}

static describe_outcome put_reached(writing *state, const char *definition) {
    const char *outer = state->prefix;
    const char *outer_answered_by = state->answered_by;
    char *prefix = nested_prefix(state, definition);
    const char **slot;
    describe_outcome outcome;
    if (prefix == NULL) {
        return DESCRIBE_OUT_OF_MEMORY;
    }
    state->prefix = prefix;
    state->answered_by = NULL;
    outcome = put_described(state, definition);
    state->prefix = outer;
    state->answered_by = outer_answered_by;
    weivel_standard_memory_release(prefix);
    if (outcome != DESCRIBE_DONE) {
        return outcome;
    }
    DESCRIBE_APPEND(state->written, state->written_count, slot);
    if (slot == NULL) {
        return DESCRIBE_OUT_OF_MEMORY;
    }
    *slot = describe_model_type(state->model, definition)->name;
    return DESCRIBE_DONE;
}

static describe_outcome put_source(writing *state) {
    describe_outcome outcome;
    put_head(state);
    outcome = put_described(state, state->service->definition);
    if (outcome == DESCRIBE_DONE) {
        put_symbol(state);
    }
    return outcome;
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
    if (outcome == DESCRIBE_DONE && service->answered_by != NULL) {
        outcome = check_identifier(service->answered_by, "answering function", refusal);
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

static void release_writing(writing *state) {
    size_t at;
    for (at = 0; at < state->filled_count; at++) {
        weivel_standard_memory_release(state->filled[at].prefix);
    }
    weivel_standard_memory_release(state->filled);
    weivel_standard_memory_release((void *)state->written);
    weivel_standard_memory_release((void *)state->open);
}

describe_outcome describe_write(const describe_model *model, const describe_service *service, describe_text *source,
                                describe_refusal *refusal) {
    writing state;
    describe_outcome outcome;
    memset(source, 0, sizeof *source);
    memset(refusal, 0, sizeof *refusal);
    outcome = check_service(model, service, refusal);
    if (outcome != DESCRIBE_DONE) {
        return outcome;
    }
    memset(&state, 0, sizeof state);
    state.source = source;
    state.service = service;
    state.model = model;
    state.refusal = refusal;
    state.prefix = service->prefix;
    state.answered_by = service->answered_by;
    outcome = put_source(&state);
    release_writing(&state);
    if (state.failed) {
        describe_refusal_release(refusal);
        outcome = DESCRIBE_OUT_OF_MEMORY;
    }
    if (outcome != DESCRIBE_DONE) {
        describe_text_release(source);
    }
    return outcome;
}

void describe_text_release(describe_text *text) {
    weivel_standard_memory_release(text->bytes);
    memset(text, 0, sizeof *text);
}
