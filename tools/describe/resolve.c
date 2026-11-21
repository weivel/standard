#include <stdio.h>
#include <string.h>

#include "reading.h"
#include "tokens.h"

#define GRAMMAR " is not in weivel's description grammar"
#define KIND(name) WEIVEL_STANDARD_DESCRIBED_##name

enum { STORAGE_UNKNOWN = 0, STORAGE_HELD = 1, STORAGE_NOT_HELD = 2 };

/** `visited` and `holds` have one byte per type: whether the current walk entered it, and its memoised answer. */
typedef struct resolving {
    describe_model *model;
    const describe_names *names;
    describe_refusal *refusal;
    unsigned char *visited;
    unsigned char *holds;
} resolving;

typedef enum referent {
    REFERENT_UNKNOWN,
    REFERENT_SCALAR,
    REFERENT_CHAR,
    REFERENT_BYTE,
    REFERENT_VOID,
    REFERENT_STRUCT,
    REFERENT_VALUE,
    REFERENT_TREE,
    REFERENT_HANDLE,
    REFERENT_ENUM,
    REFERENT_DEFINITION
} referent;

/** A run of text: a doc or a part of one. */
typedef struct text_span {
    const char *start;
    size_t length;
} text_span;

static size_t put(char *text, size_t at, const char *part) {
    size_t length = strlen(part);
    memcpy(text + at, part, length);
    return at + length;
}

char *describe_spelling_text(const describe_spelling *spelling) {
    size_t length = strlen(spelling->base) + (spelling->base_const ? 6 : 0) + 8 * (size_t)spelling->pointers + 1;
    char *text = weivel_standard_memory_allocate(length);
    size_t at = 0;
    uint32_t pointer;
    if (text == NULL) {
        return NULL;
    }
    if (spelling->base_const) {
        at = put(text, at, "const ");
    }
    at = put(text, at, spelling->base);
    for (pointer = 0; pointer < spelling->pointers; pointer++) {
        at = put(text, at, pointer == 0 ? " *" : "*");
        if (spelling->const_pointers & ((uint32_t)1 << pointer)) {
            at = put(text, at, "const ");
        }
    }
    text[at] = '\0';
    return text;
}

static uint32_t scalar_kind(const char *base) {
    if (strcmp(base, "uint32_t") == 0) {
        return KIND(U32);
    }
    if (strcmp(base, "uint64_t") == 0) {
        return KIND(U64);
    }
    if (strcmp(base, "int64_t") == 0) {
        return KIND(I64);
    }
    if (strcmp(base, "double") == 0) {
        return KIND(F64);
    }
    return strcmp(base, "size_t") == 0 ? KIND(SIZE) : KIND(NONE);
}

static int is_elsewhere(const describe_names *names, const char *base) {
    size_t at;
    for (at = 0; at < names->elsewhere_count; at++) {
        if (strcmp(names->elsewhere[at], base) == 0) {
            return 1;
        }
    }
    return 0;
}

static referent referent_of(const resolving *state, const char *base) {
    const describe_type *type;
    if (scalar_kind(base) != KIND(NONE)) {
        return REFERENT_SCALAR;
    }
    if (strcmp(base, "char") == 0) {
        return REFERENT_CHAR;
    }
    if (strcmp(base, "uint8_t") == 0) {
        return REFERENT_BYTE;
    }
    if (strcmp(base, "void") == 0) {
        return REFERENT_VOID;
    }
    if (strcmp(base, state->names->value) == 0) {
        return REFERENT_VALUE;
    }
    if (strcmp(base, state->names->tree) == 0) {
        return REFERENT_TREE;
    }
    if (is_elsewhere(state->names, base)) {
        return REFERENT_DEFINITION;
    }
    type = describe_model_type(state->model, base);
    if (type != NULL) {
        return type->definition ? REFERENT_DEFINITION : REFERENT_STRUCT;
    }
    if (describe_model_handle(state->model, base) != NULL) {
        return REFERENT_HANDLE;
    }
    return describe_model_enum(state->model, base) != NULL ? REFERENT_ENUM : REFERENT_UNKNOWN;
}

static int spelled(const describe_spelling *spelling, uint32_t base_const, uint32_t pointers, uint32_t const_pointers) {
    return spelling->base_const == base_const && spelling->pointers == pointers &&
           spelling->const_pointers == const_pointers;
}

static int is_const_pointer(const describe_spelling *spelling) {
    return spelled(spelling, 1, 1, 0);
}

static int is_mutable_pointer(const describe_spelling *spelling) {
    return spelled(spelling, 0, 1, 0);
}

static int is_by_value(const describe_spelling *spelling) {
    return spelled(spelling, 0, 0, 0);
}

static int is_texts(const describe_spelling *spelling) {
    return spelled(spelling, 1, 2, 1) && strcmp(spelling->base, "char") == 0;
}

static int is_size(const describe_item *item) {
    return is_by_value(&item->spelling) && strcmp(item->spelling.base, "size_t") == 0;
}

static int is_leading_size(const describe_item *item) {
    return is_by_value(&item->spelling) && strcmp(item->spelling.base, "uint32_t") == 0 &&
           strcmp(item->name, "size") == 0;
}

static int is_context(const describe_item *item) {
    return is_mutable_pointer(&item->spelling) && strcmp(item->spelling.base, "void") == 0 &&
           strcmp(item->name, "context") == 0;
}

static describe_outcome set_name(char **into, const char *name) {
    weivel_standard_memory_release(*into);
    *into = describe_copy_text(name);
    return *into == NULL ? DESCRIBE_OUT_OF_MEMORY : DESCRIBE_DONE;
}

static describe_outcome set_kind(describe_item *item, uint32_t kind, uint32_t role, uint32_t element,
                                 const char *type) {
    item->kind = kind;
    item->role = role;
    item->element = element;
    return type == NULL ? DESCRIBE_DONE : set_name(&item->type, type);
}

static describe_outcome refuse_spelled(resolving *state, const describe_item *item, const char *what) {
    char *spelling = describe_spelling_text(&item->spelling);
    describe_outcome outcome;
    if (spelling == NULL) {
        return DESCRIBE_OUT_OF_MEMORY;
    }
    if (referent_of(state, item->spelling.base) == REFERENT_UNKNOWN) {
        outcome = describe_refuse(state->refusal, item->where.file, item->where.line,
                                  "`%s`, the type of the %s `%s`, which no scanned header declares,%s",
                                  item->spelling.base, what, item->name, GRAMMAR);
    } else {
        outcome = describe_refuse(state->refusal, item->where.file, item->where.line, "the %s `%s` of type `%s`%s",
                                  what, item->name, spelling, GRAMMAR);
    }
    weivel_standard_memory_release(spelling);
    return outcome;
}

static describe_outcome refuse_enum_by_value(resolving *state, const describe_item *item, const char *what) {
    return describe_refuse(state->refusal, item->where.file, item->where.line,
                           "the %s `%s` holds the enum `%s` as its own type, whose size C leaves to the compiler; hold "
                           "it as a uint32_t whose doc names `%s`",
                           what, item->name, item->spelling.base, item->spelling.base);
}

static describe_outcome resolve_pointed(resolving *state, describe_item *item, uint32_t role, const char *what) {
    const char *base = item->spelling.base;
    switch (referent_of(state, base)) {
    case REFERENT_CHAR:
        return set_kind(item, KIND(TEXT), role, KIND(NONE), NULL);
    case REFERENT_BYTE:
        return set_kind(item, KIND(BYTES), role, KIND(NONE), NULL);
    case REFERENT_STRUCT:
        if (strcmp(base, state->names->refusal) == 0) {
            return describe_refuse(state->refusal, item->where.file, item->where.line,
                                   "the %s `%s` points at the refusal type `%s` as an input%s", what, item->name, base,
                                   GRAMMAR);
        }
        return set_kind(item, KIND(POINTER), role, KIND(STRUCT), base);
    case REFERENT_VALUE:
        return set_kind(item, KIND(POINTER), role, KIND(VALUE), NULL);
    case REFERENT_SCALAR:
        return set_kind(item, KIND(ARRAY), role, scalar_kind(base), NULL);
    case REFERENT_HANDLE:
        return set_kind(item, KIND(HANDLE), role, KIND(NONE), base);
    case REFERENT_DEFINITION:
        if (role != KIND(NO_ROLE)) {
            return describe_refuse(state->refusal, item->where.file, item->where.line,
                                   "the %s `%s` points at the definition `%s` as an input, where a definition is "
                                   "held by a member or handed out as an answer%s",
                                   what, item->name, base, GRAMMAR);
        }
        return set_kind(item, KIND(DEFINITION), role, KIND(NONE), base);
    default:
        return refuse_spelled(state, item, what);
    }
}

static describe_outcome resolve_by_value(resolving *state, describe_item *item, uint32_t role, const char *what) {
    switch (referent_of(state, item->spelling.base)) {
    case REFERENT_SCALAR:
        return set_kind(item, scalar_kind(item->spelling.base), role, KIND(NONE), NULL);
    case REFERENT_ENUM:
        return refuse_enum_by_value(state, item, what);
    default:
        return refuse_spelled(state, item, what);
    }
}

static describe_outcome resolve_member(resolving *state, const describe_type *type, describe_item *member) {
    const describe_spelling *spelling = &member->spelling;
    referent pointed = referent_of(state, spelling->base);
    if (is_by_value(spelling) && pointed == REFERENT_STRUCT) {
        return set_kind(member, KIND(STRUCT), KIND(NO_ROLE), KIND(NONE), spelling->base);
    }
    if (is_by_value(spelling) && pointed == REFERENT_VALUE) {
        return set_kind(member, KIND(VALUE), KIND(NO_ROLE), KIND(NONE), NULL);
    }
    if (is_by_value(spelling)) {
        return resolve_by_value(state, member, KIND(NO_ROLE), "member");
    }
    if (is_texts(spelling)) {
        return set_kind(member, KIND(TEXTS), KIND(NO_ROLE), KIND(NONE), NULL);
    }
    if (is_const_pointer(spelling)) {
        return resolve_pointed(state, member, KIND(NO_ROLE), "member");
    }
    if (is_mutable_pointer(spelling) && pointed == REFERENT_HANDLE) {
        return set_kind(member, KIND(HANDLE), KIND(NO_ROLE), KIND(NONE), spelling->base);
    }
    if (is_mutable_pointer(spelling) && pointed == REFERENT_VOID && strcmp(member->name, "storage") == 0) {
        return set_kind(member, KIND(STORAGE), KIND(NO_ROLE), KIND(NONE), NULL);
    }
    if (is_context(member) && type->definition) {
        return set_kind(member, KIND(CONTEXT), KIND(NO_ROLE), KIND(NONE), NULL);
    }
    if (is_context(member)) {
        return describe_refuse(state->refusal, member->where.file, member->where.line,
                               "`void *context` outside a definition%s", GRAMMAR);
    }
    return refuse_spelled(state, member, "member");
}

static describe_outcome resolve_answer(resolving *state, describe_item *parameter, int last) {
    const char *base = parameter->spelling.base;
    switch (referent_of(state, base)) {
    case REFERENT_STRUCT:
        if (strcmp(base, state->names->refusal) == 0) {
            return last ? set_kind(parameter, KIND(POINTER), KIND(REFUSAL), KIND(STRUCT), base)
                        : describe_refuse(state->refusal, parameter->where.file, parameter->where.line,
                                          "the refusal `%s` before the last parameter%s", parameter->name, GRAMMAR);
        }
        return set_kind(parameter, KIND(POINTER), KIND(ANSWER), KIND(STRUCT), base);
    case REFERENT_TREE:
        return set_kind(parameter, KIND(VALUE), KIND(ANSWER), KIND(NONE), NULL);
    case REFERENT_SCALAR:
        return set_kind(parameter, scalar_kind(base), KIND(ANSWER), KIND(NONE), NULL);
    case REFERENT_HANDLE:
        return set_kind(parameter, KIND(HANDLE), KIND(INPUT), KIND(NONE), base);
    default:
        return refuse_spelled(state, parameter, "parameter");
    }
}

static describe_outcome resolve_parameter(resolving *state, describe_item *parameter, int last) {
    const describe_spelling *spelling = &parameter->spelling;
    referent pointed = referent_of(state, spelling->base);
    if (is_by_value(spelling)) {
        return resolve_by_value(state, parameter, KIND(INPUT), "parameter");
    }
    if (spelled(spelling, 1, 2, 0) && pointed == REFERENT_CHAR) {
        return set_kind(parameter, KIND(TEXT), KIND(ANSWER), KIND(NONE), NULL);
    }
    if (spelled(spelling, 0, 2, 0) && pointed == REFERENT_HANDLE) {
        return set_kind(parameter, KIND(HANDLE), KIND(ANSWER), KIND(NONE), spelling->base);
    }
    if (spelled(spelling, 1, 2, 0) && pointed == REFERENT_DEFINITION) {
        return set_kind(parameter, KIND(DEFINITION), KIND(ANSWER), KIND(NONE), spelling->base);
    }
    if (is_texts(spelling)) {
        return set_kind(parameter, KIND(TEXTS), KIND(INPUT), KIND(NONE), NULL);
    }
    if (is_const_pointer(spelling)) {
        return resolve_pointed(state, parameter, KIND(INPUT), "parameter");
    }
    if (is_mutable_pointer(spelling)) {
        return resolve_answer(state, parameter, last);
    }
    return refuse_spelled(state, parameter, "parameter");
}

static describe_outcome resolve_freed(resolving *state, describe_operation *release) {
    describe_item *freed = &release->parameters[1];
    const char *base = freed->spelling.base;
    describe_outcome outcome;
    if (!is_mutable_pointer(&freed->spelling)) {
        return refuse_spelled(state, freed, "parameter");
    }
    switch (referent_of(state, base)) {
    case REFERENT_STRUCT:
        outcome = strcmp(base, state->names->refusal) == 0
                      ? set_kind(freed, KIND(POINTER), KIND(REFUSAL), KIND(STRUCT), base)
                      : set_kind(freed, KIND(POINTER), KIND(ANSWER), KIND(STRUCT), base);
        break;
    case REFERENT_TREE:
        outcome = set_kind(freed, KIND(VALUE), KIND(ANSWER), KIND(NONE), NULL);
        break;
    case REFERENT_HANDLE:
        outcome = set_kind(freed, KIND(HANDLE), KIND(INPUT), KIND(NONE), base);
        if (outcome == DESCRIBE_DONE) {
            outcome = set_name(&freed->release, release->name);
        }
        break;
    default:
        return refuse_spelled(state, freed, "parameter");
    }
    return outcome == DESCRIBE_DONE ? set_name(&release->frees, base) : outcome;
}

static describe_outcome resolve_returns(resolving *state, describe_operation *operation) {
    const describe_spelling *returned = &operation->returned;
    char *spelling;
    describe_outcome outcome;
    if (is_by_value(returned) && strcmp(returned->base, state->names->status) == 0) {
        operation->returns = WEIVEL_STANDARD_DESCRIBED_STATUS;
        return DESCRIBE_DONE;
    }
    if (is_by_value(returned) && strcmp(returned->base, "void") == 0) {
        operation->returns = WEIVEL_STANDARD_DESCRIBED_NOTHING;
        return DESCRIBE_DONE;
    }
    spelling = describe_spelling_text(returned);
    if (spelling == NULL) {
        return DESCRIBE_OUT_OF_MEMORY;
    }
    outcome = describe_refuse(state->refusal, operation->where.file, operation->where.line,
                              "the operation `%s` returning `%s`, neither `%s` nor void,%s", operation->name, spelling,
                              state->names->status, GRAMMAR);
    weivel_standard_memory_release(spelling);
    return outcome;
}

/** Whether a doc carries `tag` as a tag of its own, `@<name>` ending at a space or the doc's end. */
static int carries_tag(const char *doc, const char *tag) {
    size_t length = strlen(tag);
    size_t at;
    for (at = 0; doc != NULL && doc[at] != '\0'; at++) {
        if (doc[at] == '@' && (at == 0 || doc[at - 1] == ' ') && strncmp(doc + at, tag, length) == 0 &&
            (doc[at + length] == ' ' || doc[at + length] == '\0')) {
            return 1;
        }
    }
    return 0;
}

static describe_outcome resolve_blocking(resolving *state, describe_operation *operation) {
    operation->blocking = carries_tag(operation->doc, "@blocking");
    if (operation->blocking && operation->returns == WEIVEL_STANDARD_DESCRIBED_NOTHING) {
        return describe_refuse(state->refusal, operation->where.file, operation->where.line,
                               "the release `%s` is tagged `@blocking`, and a release frees what it is handed without "
                               "waiting; take the tag off it",
                               operation->name);
    }
    return DESCRIBE_DONE;
}

static describe_outcome resolve_operation(resolving *state, describe_operation *operation) {
    describe_outcome outcome = resolve_returns(state, operation);
    size_t at;
    if (outcome == DESCRIBE_DONE) {
        outcome = resolve_blocking(state, operation);
    }
    if (outcome != DESCRIBE_DONE) {
        return outcome;
    }
    if (operation->parameter_count == 0 || !is_context(&operation->parameters[0])) {
        return describe_refuse(state->refusal, operation->where.file, operation->where.line,
                               "the operation `%s`, whose first parameter is not `void *context`,%s", operation->name,
                               GRAMMAR);
    }
    outcome = set_kind(&operation->parameters[0], KIND(CONTEXT), KIND(INPUT), KIND(NONE), NULL);
    if (operation->returns == WEIVEL_STANDARD_DESCRIBED_NOTHING) {
        if (operation->parameter_count != 2) {
            return describe_refuse(state->refusal, operation->where.file, operation->where.line,
                                   "the release `%s`, taking other than its context and one pointer,%s",
                                   operation->name, GRAMMAR);
        }
        return outcome == DESCRIBE_DONE ? resolve_freed(state, operation) : outcome;
    }
    for (at = 1; outcome == DESCRIBE_DONE && at < operation->parameter_count; at++) {
        outcome = resolve_parameter(state, &operation->parameters[at], at + 1 == operation->parameter_count);
    }
    return outcome;
}

static describe_outcome refuse_definition(resolving *state, const describe_type *type) {
    if (type->member_count > 2) {
        const describe_item *extra = &type->members[2];
        return describe_refuse(state->refusal, extra->where.file, extra->where.line,
                               "the member `%s` of the definition `%s`, beside its size and context,%s", extra->name,
                               type->name, GRAMMAR);
    }
    return describe_refuse(state->refusal, type->where.file, type->where.line,
                           "the definition `%s`, not beginning `uint32_t size; void *context;`,%s", type->name,
                           GRAMMAR);
}

static int names_a_part(const describe_type *type, const char *name) {
    size_t at;
    for (at = 0; at < type->member_count; at++) {
        if (strcmp(type->members[at].name, name) == 0) {
            return 1;
        }
    }
    return describe_type_operation(type, name) != NULL;
}

#define FIRST_SHAPE "irst shape ends at `"

/** The next "First shape ends at `" or "first shape ends at `", as a sentence or after "Its", from `from`. */
static const char *find_first_shape(const char *doc, const char *from) {
    const char *found = strstr(from, FIRST_SHAPE);
    while (found != NULL && (found == doc || (found[-1] != 'F' && found[-1] != 'f'))) {
        found = strstr(found + 1, FIRST_SHAPE);
    }
    return found;
}

/** The member a doc's first shape line names, into `*name`; refuses two lines that disagree or one never closed. */
static describe_outcome read_first_shape(resolving *state, describe_type *type, text_span *name) {
    const char *found = type->doc == NULL ? NULL : find_first_shape(type->doc, type->doc);
    name->start = NULL;
    name->length = 0;
    while (found != NULL) {
        const char *start = found + strlen(FIRST_SHAPE);
        const char *end = strchr(start, '`');
        if (end == NULL || end == start) {
            return describe_refuse(state->refusal, type->where.file, type->where.line,
                                   "the first shape line of `%s`, naming no member between backticks,%s", type->name,
                                   GRAMMAR);
        }
        if (name->start != NULL &&
            ((size_t)(end - start) != name->length || memcmp(start, name->start, name->length) != 0)) {
            return describe_refuse(state->refusal, type->where.file, type->where.line,
                                   "`%s` names two first shapes, `%.*s` and `%.*s`", type->name, (int)name->length,
                                   name->start, (int)(end - start), start);
        }
        name->start = start;
        name->length = (size_t)(end - start);
        found = find_first_shape(type->doc, end);
    }
    return DESCRIBE_DONE;
}

static describe_outcome resolve_shape(resolving *state, describe_type *type) {
    text_span shape;
    describe_outcome outcome;
    type->definition = type->operation_count > 0;
    type->sized = type->member_count > 0 && is_leading_size(&type->members[0]);
    type->elsewhere = is_elsewhere(state->names, type->name);
    if (type->elsewhere && !type->definition) {
        return describe_refuse(state->refusal, type->where.file, type->where.line,
                               "`%s` is named as described elsewhere and is declared here as a struct that is no "
                               "definition; name only definitions as described elsewhere",
                               type->name);
    }
    if (type->definition && (type->member_count != 2 || !type->sized || !is_context(&type->members[1]))) {
        return refuse_definition(state, type);
    }
    outcome = read_first_shape(state, type, &shape);
    if (outcome != DESCRIBE_DONE) {
        return outcome;
    }
    if (type->sized && shape.start == NULL) {
        return describe_refuse(state->refusal, type->where.file, type->where.line,
                               "`%s` begins with `uint32_t size` and its doc names no first shape: say where it ends, "
                               "`First shape ends at `<member>``",
                               type->name);
    }
    if (!type->sized && shape.start != NULL) {
        return describe_refuse(state->refusal, type->where.file, type->where.line,
                               "`%s` names a first shape and does not begin with `uint32_t size`", type->name);
    }
    if (shape.start == NULL) {
        return DESCRIBE_DONE;
    }
    type->first_shape = weivel_standard_memory_allocate(shape.length + 1);
    if (type->first_shape == NULL) {
        return DESCRIBE_OUT_OF_MEMORY;
    }
    memcpy(type->first_shape, shape.start, shape.length);
    type->first_shape[shape.length] = '\0';
    return names_a_part(type, type->first_shape)
               ? DESCRIBE_DONE
               : describe_refuse(state->refusal, type->where.file, type->where.line,
                                 "the first shape of `%s` ends at `%s`, which it does not declare", type->name,
                                 type->first_shape);
}

#define CANDIDATES 5

static int ends_with(const char *name, size_t length, const char *ending) {
    size_t ending_length = strlen(ending);
    return length > ending_length && memcmp(name + length - ending_length, ending, ending_length) == 0;
}

/** Appends `stem` (its first `stem_length` bytes) then `suffix` to `candidates`; 0 when memory runs out. */
static int add_candidate(char *candidates[CANDIDATES], size_t *at, const char *stem, size_t stem_length,
                         const char *suffix) {
    size_t suffix_length = strlen(suffix);
    char *candidate = weivel_standard_memory_allocate(stem_length + suffix_length + 1);
    if (candidate == NULL) {
        return 0;
    }
    memcpy(candidate, stem, stem_length);
    memcpy(candidate + stem_length, suffix, suffix_length + 1);
    candidates[(*at)++] = candidate;
    return 1;
}

/** Every item that could count `item`, by the naming rule of reader.h, as `candidates`, NULL-terminated. */
static int count_names(const describe_item *item, char *candidates[CANDIDATES]) {
    const char *name = item->name;
    size_t length = strlen(name);
    size_t at = 0;
    memset(candidates, 0, CANDIDATES * sizeof *candidates);
    if (item->kind == KIND(BYTES)) {
        return add_candidate(candidates, &at, name, length, "_length") &&
               add_candidate(candidates, &at, "", 0, "length");
    }
    if (!add_candidate(candidates, &at, name, length, "_count")) {
        return 0;
    }
    if (ends_with(name, length, "ies") && !add_candidate(candidates, &at, name, length - 3, "y_count")) {
        return 0;
    }
    if (ends_with(name, length, "s") && !add_candidate(candidates, &at, name, length - 1, "_count")) {
        return 0;
    }
    return add_candidate(candidates, &at, "", 0, "count");
}

static void release_names(char *candidates[CANDIDATES]) {
    size_t at;
    for (at = 0; at < CANDIDATES; at++) {
        weivel_standard_memory_release(candidates[at]);
    }
}

/** The candidates as "`a`, `b` or `c`"; NULL when memory runs out. */
static char *candidate_list(char *candidates[CANDIDATES]) {
    size_t count = 0;
    size_t length = 1;
    size_t at;
    size_t written = 0;
    char *list;
    while (count < CANDIDATES && candidates[count] != NULL) {
        length += strlen(candidates[count]) + 6;
        count++;
    }
    list = weivel_standard_memory_allocate(length);
    if (list == NULL) {
        return NULL;
    }
    for (at = 0; at < count; at++) {
        const char *separator = at == 0 ? "`" : at + 1 == count ? " or `" : ", `";
        size_t name_length = strlen(candidates[at]);
        memcpy(list + written, separator, strlen(separator));
        written += strlen(separator);
        memcpy(list + written, candidates[at], name_length);
        written += name_length;
        list[written++] = '`';
    }
    list[written] = '\0';
    return list;
}

static describe_outcome refuse_uncounted(resolving *state, const describe_item *item, char *candidates[CANDIDATES]) {
    char *list = candidate_list(candidates);
    describe_outcome outcome;
    if (list == NULL) {
        return DESCRIBE_OUT_OF_MEMORY;
    }
    outcome = describe_refuse(state->refusal, item->where.file, item->where.line,
                              "`%s` has no size_t beside it counting it, named %s", item->name, list);
    weivel_standard_memory_release(list);
    return outcome;
}

static int may_be_counted(const describe_item *item) {
    return item->kind == KIND(TEXTS) || item->kind == KIND(BYTES) || item->kind == KIND(ARRAY) ||
           (item->kind == KIND(POINTER) && item->role != KIND(ANSWER) && item->role != KIND(REFUSAL));
}

static describe_outcome find_count(resolving *state, describe_item *items, size_t count, describe_item *item,
                                   char *candidates[CANDIDATES], const describe_item **counting) {
    size_t at;
    size_t candidate;
    *counting = NULL;
    for (candidate = 0; candidate < CANDIDATES && candidates[candidate] != NULL; candidate++) {
        for (at = 0; at < count; at++) {
            const describe_item *other = &items[at];
            if (other == item || strcmp(other->name, candidates[candidate]) != 0) {
                continue;
            }
            if (!is_size(other)) {
                return describe_refuse(state->refusal, other->where.file, other->where.line,
                                       "`%s` would count `%s` by its name and is not a size_t", other->name,
                                       item->name);
            }
            if (*counting != NULL) {
                return describe_refuse(state->refusal, item->where.file, item->where.line,
                                       "`%s` could be counted by both `%s` and `%s`; keep one", item->name,
                                       (*counting)->name, other->name);
            }
            *counting = other;
        }
    }
    return DESCRIBE_DONE;
}

static describe_outcome count_one(resolving *state, describe_item *items, size_t count, describe_item *item) {
    char *candidates[CANDIDATES];
    const describe_item *counting = NULL;
    describe_outcome outcome = count_names(item, candidates)
                                   ? find_count(state, items, count, item, candidates, &counting)
                                   : DESCRIBE_OUT_OF_MEMORY;
    if (outcome == DESCRIBE_DONE && counting == NULL && item->kind != KIND(POINTER)) {
        outcome = refuse_uncounted(state, item, candidates);
    }
    if (outcome == DESCRIBE_DONE && counting != NULL) {
        if (item->kind == KIND(POINTER)) {
            item->kind = KIND(ARRAY);
        }
        outcome = set_name(&item->count, counting->name);
    }
    release_names(candidates);
    return outcome;
}

static describe_outcome refuse_counted_twice(resolving *state, const describe_item *items, size_t count) {
    size_t at;
    size_t other;
    for (at = 0; at < count; at++) {
        for (other = at + 1; items[at].count != NULL && other < count; other++) {
            if (items[other].count != NULL && strcmp(items[at].count, items[other].count) == 0) {
                return describe_refuse(state->refusal, items[other].where.file, items[other].where.line,
                                       "`%s` counts both `%s` and `%s`, and a size_t counts one thing",
                                       items[at].count, items[at].name, items[other].name);
            }
        }
    }
    return DESCRIBE_DONE;
}

static int counts_something(const describe_item *items, size_t count, const char *name) {
    size_t at;
    for (at = 0; at < count; at++) {
        if (items[at].count != NULL && strcmp(items[at].count, name) == 0) {
            return 1;
        }
    }
    return 0;
}

/** `<x>_length` beside a text `<x>`, which a text carries as no count of its own. */
static int is_text_length(const describe_item *items, size_t count, const describe_item *size) {
    size_t length = strlen(size->name);
    size_t at;
    if (!ends_with(size->name, length, "_length")) {
        return 0;
    }
    for (at = 0; at < count; at++) {
        if (items[at].kind == KIND(TEXT) && strlen(items[at].name) == length - 7 &&
            memcmp(items[at].name, size->name, length - 7) == 0) {
            return 1;
        }
    }
    return 0;
}

/** A size_t named `<x>_count` or `<x>_length` must count something, so a name that misses its array is never read. */
static describe_outcome refuse_counting_nothing(resolving *state, const describe_item *items, size_t count) {
    size_t at;
    for (at = 0; at < count; at++) {
        const describe_item *size = &items[at];
        size_t length = strlen(size->name);
        if (size->kind != KIND(SIZE) || size->role == KIND(ANSWER) ||
            !(ends_with(size->name, length, "_count") || ends_with(size->name, length, "_length"))) {
            continue;
        }
        if (!counts_something(items, count, size->name) && !is_text_length(items, count, size)) {
            return describe_refuse(state->refusal, size->where.file, size->where.line,
                                   "`%s` is named as a count and counts nothing beside it; name the array, texts or "
                                   "bytes it counts by reader.h's rule, or rename it",
                                   size->name);
        }
    }
    return DESCRIBE_DONE;
}

static describe_outcome resolve_counts(resolving *state, describe_item *items, size_t count) {
    describe_outcome outcome = DESCRIBE_DONE;
    size_t at;
    for (at = 0; outcome == DESCRIBE_DONE && at < count; at++) {
        if (may_be_counted(&items[at])) {
            outcome = count_one(state, items, count, &items[at]);
        }
    }
    if (outcome == DESCRIBE_DONE) {
        outcome = refuse_counted_twice(state, items, count);
    }
    return outcome == DESCRIBE_DONE ? refuse_counting_nothing(state, items, count) : outcome;
}

static int is_word_part(char character) {
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
           (character >= '0' && character <= '9') || character == '_';
}

static int span_holds_word(text_span span, const char *word) {
    size_t length = strlen(word);
    size_t at;
    for (at = 0; length > 0 && at + length <= span.length; at++) {
        if (memcmp(span.start + at, word, length) == 0 && (at == 0 || !is_word_part(span.start[at - 1])) &&
            (at + length == span.length || !is_word_part(span.start[at + length]))) {
            return 1;
        }
    }
    return 0;
}

/** How many enums of the model `span` names, stopping at two, the first two into `named`. */
static size_t enums_named(const describe_model *model, text_span span, const char *named[2]) {
    size_t found = 0;
    size_t at;
    for (at = 0; at < model->enum_count && found < 2; at++) {
        if (span_holds_word(span, model->enums[at].name)) {
            named[found++] = model->enums[at].name;
        }
    }
    return found;
}

static int is_enum_candidate(const describe_item *items, const describe_item *item) {
    return item->kind == KIND(U32) && !(item == &items[0] && is_leading_size(item));
}

static describe_outcome bind(resolving *state, describe_item *item, const char *named) {
    if (item->type != NULL && strcmp(item->type, named) != 0) {
        return describe_refuse(state->refusal, item->where.file, item->where.line,
                               "`%s` is said to hold both `%s` and `%s`; name one enum for it", item->name, item->type,
                               named);
    }
    return item->type != NULL ? DESCRIBE_DONE : set_name(&item->type, named);
}

/** An own doc naming an enum its item cannot hold, which would otherwise describe the item as if it named nothing. */
static describe_outcome refuse_enum_unheld(resolving *state, const describe_item *item, const char *named) {
    if (item->kind == KIND(ARRAY) && item->element == KIND(U32)) {
        return describe_refuse(state->refusal, item->where.file, item->where.line,
                               "the doc of `%s` names `%s`, and `%s` is an array of uint32_t, whose elements a "
                               "description never binds to an enum; describe them without naming `%s`, or hold each "
                               "in a struct whose uint32_t member's own doc names it",
                               item->name, named, item->name, named);
    }
    return describe_refuse(state->refusal, item->where.file, item->where.line,
                           "the doc of `%s` names `%s`, and `%s` is no uint32_t, the one kind that holds an enum; "
                           "name `%s` in the own doc of the uint32_t holding it",
                           item->name, named, item->name, named);
}

static describe_outcome bind_own(resolving *state, describe_item *items, size_t count) {
    describe_outcome outcome = DESCRIBE_DONE;
    size_t at;
    for (at = 0; outcome == DESCRIBE_DONE && at < count; at++) {
        describe_item *item = &items[at];
        const char *named[2] = {NULL, NULL};
        text_span doc;
        size_t found;
        if (item->doc == NULL) {
            continue;
        }
        doc.start = item->doc;
        doc.length = strlen(item->doc);
        found = enums_named(state->model, doc, named);
        if (found > 0 && !is_enum_candidate(items, item)) {
            outcome = refuse_enum_unheld(state, item, named[0]);
        } else if (found == 2) {
            outcome = describe_refuse(state->refusal, item->where.file, item->where.line,
                                      "the doc of `%s` names both `%s` and `%s`; name the one enum it holds",
                                      item->name, named[0], named[1]);
        } else if (found == 1) {
            outcome = bind(state, item, named[0]);
        }
    }
    return outcome;
}

static describe_item *item_named(describe_item *items, size_t count, const char *start, size_t length) {
    size_t at;
    for (at = 0; at < count; at++) {
        if (strlen(items[at].name) == length && memcmp(items[at].name, start, length) == 0) {
            return &items[at];
        }
    }
    return NULL;
}

/** Of the items a clause backticks, how many there are and how many are enum candidates, the last one into `held`. */
static void backticked(describe_item *items, size_t count, text_span clause, size_t *named, size_t *candidates,
                       describe_item **held) {
    size_t at = 0;
    *named = 0;
    *candidates = 0;
    *held = NULL;
    while (at < clause.length) {
        const char *open = memchr(clause.start + at, '`', clause.length - at);
        const char *close;
        describe_item *item;
        if (open == NULL) {
            return;
        }
        close = memchr(open + 1, '`', clause.length - (size_t)(open + 1 - clause.start));
        if (close == NULL) {
            return;
        }
        item = item_named(items, count, open + 1, (size_t)(close - open - 1));
        if (item != NULL) {
            (*named)++;
            if (is_enum_candidate(items, item)) {
                (*candidates)++;
                *held = item;
            }
        }
        at = (size_t)(close + 1 - clause.start);
    }
}

static describe_item *only_candidate(describe_item *items, size_t count, size_t *candidates) {
    describe_item *found = NULL;
    size_t at;
    *candidates = 0;
    for (at = 0; at < count; at++) {
        if (is_enum_candidate(items, &items[at]) && items[at].type == NULL) {
            (*candidates)++;
            found = &items[at];
        }
    }
    return found;
}

typedef struct container {
    const char *name;
    describe_location where;
    describe_item *items;
    size_t count;
} container;

static describe_outcome bind_clause(resolving *state, const container *owner, text_span clause) {
    const char *named[2] = {NULL, NULL};
    size_t found = enums_named(state->model, clause, named);
    size_t mentioned;
    size_t candidates;
    describe_item *held;
    if (found == 0) {
        return DESCRIBE_DONE;
    }
    if (found == 2) {
        return describe_refuse(state->refusal, owner->where.file, owner->where.line,
                               "a clause of the doc of `%s` names both `%s` and `%s`; say which uint32_t holds each "
                               "in its own clause or its own doc",
                               owner->name, named[0], named[1]);
    }
    backticked(owner->items, owner->count, clause, &mentioned, &candidates, &held);
    if (mentioned == 0) {
        held = only_candidate(owner->items, owner->count, &candidates);
    }
    if (candidates > 1) {
        return describe_refuse(state->refusal, owner->where.file, owner->where.line,
                               "the doc of `%s` names `%s` without saying which uint32_t holds it; name it in that "
                               "member's own doc",
                               owner->name, named[0]);
    }
    return held == NULL ? DESCRIBE_DONE : bind(state, held, named[0]);
}

static int holds_enum(const container *owner, const char *named) {
    size_t at;
    for (at = 0; at < owner->count; at++) {
        if (owner->items[at].type != NULL && strcmp(owner->items[at].type, named) == 0) {
            return 1;
        }
    }
    return 0;
}

/**
 * A clause naming an enum must leave an item of its owner holding it once every clause is bound: one that backticks
 * no uint32_t, or names an enum when no uint32_t is free, would otherwise describe the uint32_t as a plain number.
 */
static describe_outcome check_clause_held(resolving *state, const container *owner, text_span clause) {
    const char *named[2] = {NULL, NULL};
    if (enums_named(state->model, clause, named) == 0 || holds_enum(owner, named[0])) {
        return DESCRIBE_DONE;
    }
    return describe_refuse(state->refusal, owner->where.file, owner->where.line,
                           "the doc of `%s` names `%s` and binds it to no uint32_t of `%s`; name `%s` in the own doc "
                           "of the uint32_t holding it",
                           owner->name, named[0], owner->name, named[0]);
}

typedef describe_outcome (*clause_pass)(resolving *state, const container *owner, text_span clause);

static int ends_clause(text_span text, size_t at) {
    return text.start[at] == ';' ||
           (text.start[at] == '.' && (at + 1 == text.length || text.start[at + 1] == ' '));
}

static describe_outcome each_clause(resolving *state, const container *owner, text_span text, clause_pass pass) {
    describe_outcome outcome = DESCRIBE_DONE;
    size_t start = 0;
    size_t at;
    for (at = 0; outcome == DESCRIBE_DONE && at <= text.length; at++) {
        if (at == text.length || ends_clause(text, at)) {
            text_span clause;
            clause.start = text.start + start;
            clause.length = at - start;
            outcome = pass(state, owner, clause);
            start = at + 1;
        }
    }
    return outcome;
}

static describe_outcome bind_clauses(resolving *state, const container *owner, text_span text) {
    describe_outcome outcome = each_clause(state, owner, text, bind_clause);
    return outcome == DESCRIBE_DONE ? each_clause(state, owner, text, check_clause_held) : outcome;
}

static describe_outcome resolve_type_enums(resolving *state, describe_type *type) {
    describe_outcome outcome = bind_own(state, type->members, type->member_count);
    container owner;
    text_span doc;
    if (outcome != DESCRIBE_DONE || type->doc == NULL) {
        return outcome;
    }
    owner.name = type->name;
    owner.where = type->where;
    owner.items = type->members;
    owner.count = type->member_count;
    doc.start = type->doc;
    doc.length = strlen(type->doc);
    return bind_clauses(state, &owner, doc);
}

static int is_tag_at(const char *doc, size_t at) {
    return doc[at] == '@' && (at == 0 || doc[at - 1] == ' ') && doc[at + 1] >= 'a' && doc[at + 1] <= 'z';
}

static size_t next_tag(const char *doc, size_t from) {
    size_t at;
    for (at = from; doc[at] != '\0'; at++) {
        if (is_tag_at(doc, at)) {
            return at;
        }
    }
    return at;
}

/** The `@param <name>` paragraph of an operation's doc, without the tag and the name; length 0 when there is none. */
static text_span parameter_paragraph(const char *doc, const char *name) {
    text_span paragraph;
    size_t length = strlen(name);
    size_t at = next_tag(doc, 0);
    paragraph.start = NULL;
    paragraph.length = 0;
    while (doc[at] != '\0') {
        size_t end = next_tag(doc, at + 1);
        if (strncmp(doc + at, "@param ", 7) == 0 && strncmp(doc + at + 7, name, length) == 0 &&
            (doc[at + 7 + length] == ' ' || doc[at + 7 + length] == '\0')) {
            paragraph.start = doc + at + 7 + length;
            paragraph.length = end - (at + 7 + length);
            return paragraph;
        }
        at = end;
    }
    return paragraph;
}

static describe_outcome copy_paragraphs(describe_operation *operation) {
    size_t at;
    for (at = 0; operation->doc != NULL && at < operation->parameter_count; at++) {
        describe_item *parameter = &operation->parameters[at];
        text_span paragraph = parameter_paragraph(operation->doc, parameter->name);
        size_t from = 0;
        if (paragraph.start == NULL) {
            continue;
        }
        while (from < paragraph.length && paragraph.start[from] == ' ') {
            from++;
        }
        while (paragraph.length > from && paragraph.start[paragraph.length - 1] == ' ') {
            paragraph.length--;
        }
        parameter->doc = weivel_standard_memory_allocate(paragraph.length - from + 1);
        if (parameter->doc == NULL) {
            return DESCRIBE_OUT_OF_MEMORY;
        }
        memcpy(parameter->doc, paragraph.start + from, paragraph.length - from);
        parameter->doc[paragraph.length - from] = '\0';
    }
    return DESCRIBE_DONE;
}

static describe_outcome resolve_operation_enums(resolving *state, describe_operation *operation) {
    describe_outcome outcome = copy_paragraphs(operation);
    container owner;
    text_span lead;
    if (outcome == DESCRIBE_DONE) {
        outcome = bind_own(state, operation->parameters, operation->parameter_count);
    }
    if (outcome != DESCRIBE_DONE || operation->doc == NULL) {
        return outcome;
    }
    owner.name = operation->name;
    owner.where = operation->where;
    owner.items = operation->parameters;
    owner.count = operation->parameter_count;
    lead.start = operation->doc;
    lead.length = next_tag(operation->doc, 0);
    return bind_clauses(state, &owner, lead);
}

static const describe_operation *release_of(const describe_type *definition, const char *freed) {
    size_t at;
    for (at = 0; at < definition->operation_count; at++) {
        const describe_operation *operation = &definition->operations[at];
        if (operation->returns == WEIVEL_STANDARD_DESCRIBED_NOTHING && strcmp(operation->frees, freed) == 0) {
            return operation;
        }
    }
    return NULL;
}

static describe_outcome refuse_second_releases(resolving *state, const describe_type *definition) {
    size_t at;
    for (at = 0; at < definition->operation_count; at++) {
        const describe_operation *operation = &definition->operations[at];
        const describe_operation *first;
        if (operation->returns != WEIVEL_STANDARD_DESCRIBED_NOTHING) {
            continue;
        }
        first = release_of(definition, operation->frees);
        if (first != operation) {
            return describe_refuse(state->refusal, operation->where.file, operation->where.line,
                                   "the release `%s` frees `%s`, as `%s` already does; a definition holds one release "
                                   "per type",
                                   operation->name, operation->frees, first->name);
        }
    }
    return DESCRIBE_DONE;
}

static int names_a_struct(const describe_item *item) {
    return item->kind == KIND(STRUCT) ||
           ((item->kind == KIND(POINTER) || item->kind == KIND(ARRAY)) && item->element == KIND(STRUCT));
}

/** Whether a struct not yet visited in this walk reaches storage, marking each struct it enters. */
static int reaches_storage(const resolving *state, const char *name) {
    const describe_type *type = describe_model_type(state->model, name);
    size_t at;
    if (type == NULL || state->visited[type - state->model->types]) {
        return 0;
    }
    state->visited[type - state->model->types] = 1;
    for (at = 0; at < type->member_count; at++) {
        const describe_item *member = &type->members[at];
        if (member->kind == KIND(STORAGE) || member->kind == KIND(HANDLE) ||
            (names_a_struct(member) && reaches_storage(state, member->type))) {
            return 1;
        }
    }
    return 0;
}

/**
 * @implNote Each answer is a fresh walk visiting every struct at most once and is memoised for its root alone: a
 * struct's answer found while an enclosing struct is still being walked can be wrong, since the cycle back to that
 * struct is cut there.
 */
static int holds_storage(const resolving *state, const char *name) {
    const describe_type *type = describe_model_type(state->model, name);
    size_t index;
    if (type == NULL) {
        return 0;
    }
    index = (size_t)(type - state->model->types);
    if (state->holds[index] == STORAGE_UNKNOWN) {
        memset(state->visited, 0, state->model->type_count);
        state->holds[index] = reaches_storage(state, name) ? STORAGE_HELD : STORAGE_NOT_HELD;
    }
    return state->holds[index] == STORAGE_HELD;
}

static int may_name_release(const describe_item *parameter) {
    return parameter->role == KIND(REFUSAL) || (parameter->role == KIND(ANSWER) && parameter->kind == KIND(POINTER));
}

/** Whether the parameter must name a release: a refusal or struct answer only when its struct holds storage. */
static int needs_release(const resolving *state, const describe_item *parameter) {
    if (parameter->kind == KIND(HANDLE) || (parameter->role == KIND(ANSWER) && parameter->kind == KIND(VALUE))) {
        return 1;
    }
    return may_name_release(parameter) && holds_storage(state, parameter->type);
}

static describe_outcome resolve_releases(resolving *state, const describe_type *definition) {
    describe_outcome outcome = refuse_second_releases(state, definition);
    size_t at;
    size_t parameter;
    for (at = 0; outcome == DESCRIBE_DONE && at < definition->operation_count; at++) {
        describe_operation *operation = &definition->operations[at];
        for (parameter = 1; operation->returns == WEIVEL_STANDARD_DESCRIBED_STATUS && outcome == DESCRIBE_DONE &&
                            parameter < operation->parameter_count;
             parameter++) {
            describe_item *item = &operation->parameters[parameter];
            const char *freed = item->kind == KIND(VALUE) ? state->names->tree : item->type;
            const describe_operation *release;
            int needed = needs_release(state, item);
            if (!needed && !may_name_release(item)) {
                continue;
            }
            release = release_of(definition, freed);
            if (release != NULL) {
                outcome = set_name(&item->release, release->name);
            } else if (needed) {
                outcome = describe_refuse(state->refusal, item->where.file, item->where.line,
                                          "`%s` of `%s` is a `%s`, which holds storage, and no release of `%s` "
                                          "frees it",
                                          item->name, operation->name, freed, definition->name);
            }
        }
    }
    return outcome;
}

static describe_outcome resolve_handle(resolving *state, describe_handle *handle) {
    size_t at;
    for (at = 0; at < state->model->type_count; at++) {
        const describe_operation *release = release_of(&state->model->types[at], handle->name);
        if (release == NULL) {
            continue;
        }
        if (handle->release != NULL && strcmp(handle->release, release->name) != 0) {
            return describe_refuse(state->refusal, release->where.file, release->where.line,
                                   "the release `%s` frees the handle `%s`, which `%s` frees elsewhere; one name frees "
                                   "a handle",
                                   release->name, handle->name, handle->release);
        }
        if (handle->release == NULL && set_name(&handle->release, release->name) != DESCRIBE_DONE) {
            return DESCRIBE_OUT_OF_MEMORY;
        }
    }
    return DESCRIBE_DONE;
}

static describe_outcome resolve_handle_members(resolving *state, describe_type *type) {
    size_t at;
    for (at = 0; at < type->member_count; at++) {
        describe_item *member = &type->members[at];
        const describe_handle *handle;
        if (member->kind != KIND(HANDLE)) {
            continue;
        }
        handle = describe_model_handle(state->model, member->type);
        if (handle->release == NULL) {
            return describe_refuse(state->refusal, member->where.file, member->where.line,
                                   "`%s` holds the handle `%s`, which no release of any definition frees",
                                   member->name, member->type);
        }
        if (set_name(&member->release, handle->release) != DESCRIBE_DONE) {
            return DESCRIBE_OUT_OF_MEMORY;
        }
    }
    return DESCRIBE_DONE;
}

static describe_outcome resolve_kinds(resolving *state, describe_type *type) {
    describe_outcome outcome = DESCRIBE_DONE;
    size_t at;
    for (at = 0; outcome == DESCRIBE_DONE && at < type->member_count; at++) {
        outcome = resolve_member(state, type, &type->members[at]);
    }
    for (at = 0; outcome == DESCRIBE_DONE && at < type->operation_count; at++) {
        outcome = resolve_operation(state, &type->operations[at]);
    }
    return outcome;
}

static describe_outcome resolve_type_counts_and_enums(resolving *state, describe_type *type) {
    describe_outcome outcome = resolve_counts(state, type->members, type->member_count);
    size_t at;
    if (outcome == DESCRIBE_DONE) {
        outcome = resolve_type_enums(state, type);
    }
    for (at = 0; outcome == DESCRIBE_DONE && at < type->operation_count; at++) {
        describe_operation *operation = &type->operations[at];
        outcome = resolve_counts(state, operation->parameters, operation->parameter_count);
        if (outcome == DESCRIBE_DONE) {
            outcome = resolve_operation_enums(state, operation);
        }
    }
    return outcome;
}

typedef describe_outcome (*type_pass)(resolving *state, describe_type *type);

static describe_outcome every_type(resolving *state, type_pass pass) {
    describe_outcome outcome = DESCRIBE_DONE;
    size_t at;
    for (at = 0; outcome == DESCRIBE_DONE && at < state->model->type_count; at++) {
        outcome = pass(state, &state->model->types[at]);
    }
    return outcome;
}

static describe_outcome resolve_definition_releases(resolving *state, describe_type *type) {
    return type->definition ? resolve_releases(state, type) : DESCRIBE_DONE;
}

describe_outcome describe_resolve(describe_model *model, const describe_names *names, describe_refusal *refusal) {
    resolving state;
    describe_outcome outcome;
    size_t at;
    state.model = model;
    state.names = names;
    state.refusal = refusal;
    state.visited = weivel_standard_memory_allocate_zeroed(model->type_count + 1, 1);
    state.holds = weivel_standard_memory_allocate_zeroed(model->type_count + 1, 1);
    outcome = state.visited == NULL || state.holds == NULL ? DESCRIBE_OUT_OF_MEMORY : every_type(&state, resolve_shape);
    if (outcome == DESCRIBE_DONE) {
        outcome = every_type(&state, resolve_kinds);
    }
    if (outcome == DESCRIBE_DONE) {
        outcome = every_type(&state, resolve_type_counts_and_enums);
    }
    if (outcome == DESCRIBE_DONE) {
        outcome = every_type(&state, resolve_definition_releases);
    }
    for (at = 0; outcome == DESCRIBE_DONE && at < model->handle_count; at++) {
        outcome = resolve_handle(&state, &model->handles[at]);
    }
    if (outcome == DESCRIBE_DONE) {
        outcome = every_type(&state, resolve_handle_members);
    }
    weivel_standard_memory_release(state.visited);
    weivel_standard_memory_release(state.holds);
    return outcome;
}
