#include "weivel/description.h"

#include "weivel/value.h"

#include "report.h"
#include "sized.h"

#include <string.h>

typedef weivel_standard_field_path field_path;
typedef weivel_standard_refusal_texts refusal_texts;
typedef weivel_standard_described_member described_member;
typedef weivel_standard_described_type described_type;
typedef weivel_standard_described_enum described_enum;
typedef weivel_standard_described_operation described_operation;
typedef weivel_standard_described_parameter described_parameter;

#define DESCRIPTION_FIRST_SHAPE (offsetof(weivel_standard_description, operation_count) + sizeof(size_t))

typedef struct checking {
    const weivel_standard_description *description;
    weivel_standard_refusal *refusal;
    weivel_status status;
} checking;

/**
 * What a member and a parameter share: the fields that refer to something else of the description. `count_kind`
 * answers the kind of the sibling a count names, NONE when there is none.
 */
typedef struct reference {
    uint32_t kind;
    uint32_t element;
    const char *type;
    const char *count;
    const char *release;
    uint32_t (*count_kind)(const void *siblings, const char *name);
    const void *siblings;
} reference;

static const char *subject_of(const char *path) {
    return path[0] == '\0' ? "the description" : path;
}

/** `what` may hold one %s, the subject; `fix` may hold one %s, the subject too. */
static int refuse(checking *state, uint32_t code, const field_path *path, const char *what, const char *fix) {
    refusal_texts texts = {NULL, NULL, NULL};
    state->status = WEIVEL_STATUS_MALFORMED;
    if (state->refusal == NULL) {
        return 0;
    }
    texts.path = weivel_standard_path_text(path);
    if (texts.path != NULL) {
        texts.detail = weivel_standard_format_text(what, subject_of(texts.path));
        texts.fix = weivel_standard_format_text(fix, subject_of(texts.path));
    }
    if (!weivel_standard_fill_refusal(state->refusal, code, texts)) {
        state->status = WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    return 0;
}

static int refuse_invalid(checking *state, const field_path *path, const char *what, const char *fix) {
    return refuse(state, WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, path, what, fix);
}

static int require_text(checking *state, const char *text, const field_path *parent, const char *key) {
    field_path path = {parent, key, 0};
    return text != NULL || refuse(state, WEIVEL_STANDARD_REFUSAL_MISSING_FIELD, &path,
                                  "%s is NULL, and a description requires it", "set %s");
}

static int require_array(checking *state, const void *items, size_t count, const field_path *parent,
                         const char *key) {
    field_path path = {parent, key, 0};
    return items != NULL || count == 0 ||
           refuse(state, WEIVEL_STANDARD_REFUSAL_MISSING_FIELD, &path,
                  "%s is NULL beside a count above zero", "point %s at its items, or set its count to 0");
}

/**
 * Whether no two of `count` items, `stride` bytes apart, share a name, refusing the later one at its name.
 *
 * @implNote Reads each name as the item's first member, which every described element begins with.
 */
static int names_once(checking *state, const void *items, size_t stride, size_t count, const field_path *parent,
                      const char *key) {
    field_path list = {parent, key, 0};
    const char *bytes = items;
    size_t at;
    size_t before;
    for (at = 1; at < count; at++) {
        const char *name = *(const char *const *)(const void *)(bytes + at * stride);
        for (before = 0; before < at; before++) {
            if (strcmp(*(const char *const *)(const void *)(bytes + before * stride), name) == 0) {
                field_path item = {&list, NULL, at};
                field_path path = {&item, "name", 0};
                return refuse(state, WEIVEL_STANDARD_REFUSAL_DUPLICATE, &path, "%s repeats a name listed before it",
                              "give each item of its list its own name, so %s names one thing");
            }
        }
    }
    return 1;
}

static int names_members(checking *state, const described_type *type, const field_path *parent) {
    field_path members = {parent, "members", 0};
    size_t at;
    if (!require_text(state, type->name, parent, "name") ||
        !require_array(state, type->members, type->member_count, parent, "members")) {
        return 0;
    }
    for (at = 0; at < type->member_count; at++) {
        field_path item = {&members, NULL, at};
        if (!require_text(state, type->members[at].name, &item, "name")) {
            return 0;
        }
    }
    return names_once(state, type->members, sizeof *type->members, type->member_count, parent, "members");
}

static int names_entries(checking *state, const described_enum *enumeration, const field_path *parent) {
    field_path entries = {parent, "entries", 0};
    size_t at;
    if (!require_text(state, enumeration->name, parent, "name") ||
        !require_array(state, enumeration->entries, enumeration->entry_count, parent, "entries")) {
        return 0;
    }
    for (at = 0; at < enumeration->entry_count; at++) {
        field_path item = {&entries, NULL, at};
        if (!require_text(state, enumeration->entries[at].name, &item, "name")) {
            return 0;
        }
    }
    return names_once(state, enumeration->entries, sizeof *enumeration->entries, enumeration->entry_count, parent,
                      "entries");
}

static int names_parameters(checking *state, const described_operation *operation, const field_path *parent) {
    field_path parameters = {parent, "parameters", 0};
    size_t at;
    if (!require_text(state, operation->name, parent, "name") ||
        !require_array(state, operation->parameters, operation->parameter_count, parent, "parameters")) {
        return 0;
    }
    for (at = 0; at < operation->parameter_count; at++) {
        field_path item = {&parameters, NULL, at};
        if (!require_text(state, operation->parameters[at].name, &item, "name")) {
            return 0;
        }
    }
    return names_once(state, operation->parameters, sizeof *operation->parameters, operation->parameter_count,
                      parent, "parameters");
}

/** Every name and array the description requires is set, so every later lookup may compare names. */
static int names_everything(checking *state, const weivel_standard_description *description) {
    field_path types = {NULL, "types", 0};
    field_path enums = {NULL, "enums", 0};
    field_path operations = {NULL, "operations", 0};
    size_t at;
    if (!require_text(state, description->definition, NULL, "definition") ||
        !require_array(state, description->types, description->type_count, NULL, "types") ||
        !require_array(state, description->enums, description->enum_count, NULL, "enums") ||
        !require_array(state, description->operations, description->operation_count, NULL, "operations")) {
        return 0;
    }
    for (at = 0; at < description->type_count; at++) {
        field_path item = {&types, NULL, at};
        if (!names_members(state, &description->types[at], &item)) {
            return 0;
        }
    }
    for (at = 0; at < description->enum_count; at++) {
        field_path item = {&enums, NULL, at};
        if (!names_entries(state, &description->enums[at], &item)) {
            return 0;
        }
    }
    for (at = 0; at < description->operation_count; at++) {
        field_path item = {&operations, NULL, at};
        if (!names_parameters(state, &description->operations[at], &item)) {
            return 0;
        }
    }
    return names_once(state, description->types, sizeof *description->types, description->type_count, NULL, "types") &&
           names_once(state, description->enums, sizeof *description->enums, description->enum_count, NULL, "enums") &&
           names_once(state, description->operations, sizeof *description->operations, description->operation_count,
                      NULL, "operations");
}

static const described_type *type_named(const weivel_standard_description *description, const char *name) {
    size_t at;
    for (at = 0; at < description->type_count; at++) {
        if (strcmp(description->types[at].name, name) == 0) {
            return &description->types[at];
        }
    }
    return NULL;
}

static const described_enum *enum_named(const weivel_standard_description *description, const char *name) {
    size_t at;
    for (at = 0; at < description->enum_count; at++) {
        if (strcmp(description->enums[at].name, name) == 0) {
            return &description->enums[at];
        }
    }
    return NULL;
}

static const described_operation *operation_named(const weivel_standard_description *description,
                                                  const char *name) {
    size_t at;
    for (at = 0; at < description->operation_count; at++) {
        if (strcmp(description->operations[at].name, name) == 0) {
            return &description->operations[at];
        }
    }
    return NULL;
}

static uint32_t member_kind(const void *siblings, const char *name) {
    const described_type *type = siblings;
    size_t at;
    for (at = 0; at < type->member_count; at++) {
        if (strcmp(type->members[at].name, name) == 0) {
            return type->members[at].kind;
        }
    }
    return WEIVEL_STANDARD_DESCRIBED_NONE;
}

static uint32_t parameter_kind(const void *siblings, const char *name) {
    const described_operation *operation = siblings;
    size_t at;
    for (at = 0; at < operation->parameter_count; at++) {
        if (strcmp(operation->parameters[at].name, name) == 0) {
            return operation->parameters[at].kind;
        }
    }
    return WEIVEL_STANDARD_DESCRIBED_NONE;
}

static int is_published_kind(uint32_t kind) {
    return kind > WEIVEL_STANDARD_DESCRIBED_NONE && kind <= WEIVEL_STANDARD_DESCRIBED_LAST;
}

static int is_counted(uint32_t kind) {
    return kind == WEIVEL_STANDARD_DESCRIBED_TEXTS || kind == WEIVEL_STANDARD_DESCRIBED_BYTES ||
           kind == WEIVEL_STANDARD_DESCRIBED_ARRAY;
}

/** What a POINTER may point at: one struct or one value node. */
static int is_pointer_element(uint32_t element) {
    return element == WEIVEL_STANDARD_DESCRIBED_STRUCT || element == WEIVEL_STANDARD_DESCRIBED_VALUE;
}

/** What an ARRAY may hold: a scalar, a struct or a value node; an array of texts is TEXTS alone. */
static int is_array_element(uint32_t element) {
    return (element >= WEIVEL_STANDARD_DESCRIBED_U32 && element <= WEIVEL_STANDARD_DESCRIBED_SIZE) ||
           is_pointer_element(element);
}

static int names_a_struct(const reference *slot) {
    return slot->kind == WEIVEL_STANDARD_DESCRIBED_STRUCT ||
           ((slot->kind == WEIVEL_STANDARD_DESCRIBED_POINTER || slot->kind == WEIVEL_STANDARD_DESCRIBED_ARRAY) &&
            slot->element == WEIVEL_STANDARD_DESCRIBED_STRUCT);
}

static int check_kind(checking *state, const reference *slot, const field_path *parent) {
    field_path kind = {parent, "kind", 0};
    field_path element = {parent, "element", 0};
    if (!is_published_kind(slot->kind)) {
        return refuse_invalid(state, &kind, "%s is not a kind weivel/description.h publishes",
                              "set %s to a weivel_standard_described_kind");
    }
    if (slot->kind == WEIVEL_STANDARD_DESCRIBED_POINTER && !is_pointer_element(slot->element)) {
        return refuse_invalid(state, &element, "%s is not STRUCT or VALUE, the elements a pointer points at",
                              "set %s to STRUCT or VALUE");
    }
    if (slot->kind == WEIVEL_STANDARD_DESCRIBED_ARRAY && !is_array_element(slot->element)) {
        return refuse_invalid(state, &element,
                              "%s is not a scalar, STRUCT or VALUE, the elements an array holds, a text array "
                              "being TEXTS",
                              "set %s to the kind of the array's elements, or describe an array of texts as TEXTS");
    }
    return 1;
}

static int check_type_name(checking *state, const reference *slot, const field_path *parent) {
    field_path type = {parent, "type", 0};
    if (names_a_struct(slot)) {
        if (!require_text(state, slot->type, parent, "type")) {
            return 0;
        }
        return type_named(state->description, slot->type) != NULL ||
               refuse_invalid(state, &type, "%s names a struct the description does not hold",
                              "add the struct %s names to the description's types");
    }
    if (slot->kind == WEIVEL_STANDARD_DESCRIBED_U32 && slot->type != NULL) {
        return enum_named(state->description, slot->type) != NULL ||
               refuse_invalid(state, &type, "%s names an enum the description does not hold",
                              "add the enum %s names to the description's enums, or set it to NULL");
    }
    return slot->kind != WEIVEL_STANDARD_DESCRIBED_HANDLE || require_text(state, slot->type, parent, "type");
}

static int check_count(checking *state, const reference *slot, const field_path *parent) {
    field_path count = {parent, "count", 0};
    uint32_t counting;
    if (!is_counted(slot->kind)) {
        return 1;
    }
    if (!require_text(state, slot->count, parent, "count")) {
        return 0;
    }
    counting = slot->count_kind(slot->siblings, slot->count);
    if (counting == WEIVEL_STANDARD_DESCRIBED_NONE) {
        return refuse_invalid(state, &count, "%s names nothing beside it", "name the size_t beside it in %s");
    }
    return counting == WEIVEL_STANDARD_DESCRIBED_SIZE ||
           refuse_invalid(state, &count, "%s names something beside it that is not of kind SIZE",
                          "name the size_t that counts it in %s");
}

static int check_release(checking *state, const reference *slot, const field_path *parent) {
    field_path release = {parent, "release", 0};
    if (slot->kind == WEIVEL_STANDARD_DESCRIBED_HANDLE && !require_text(state, slot->release, parent, "release")) {
        return 0;
    }
    return slot->release == NULL || operation_named(state->description, slot->release) != NULL ||
           refuse_invalid(state, &release, "%s names an operation the description does not hold",
                          "name the release that frees it in %s, as the description's operations list it");
}

static int check_reference(checking *state, const reference *slot, const field_path *path) {
    return check_kind(state, slot, path) && check_type_name(state, slot, path) && check_count(state, slot, path) &&
           check_release(state, slot, path);
}

/** The size a member of `kind` has in C; 0 for a kind whose size another check has already refused. */
static size_t size_of_kind(const checking *state, const described_member *member) {
    const described_type *type;
    switch (member->kind) {
    case WEIVEL_STANDARD_DESCRIBED_U32:
        return sizeof(uint32_t);
    case WEIVEL_STANDARD_DESCRIBED_U64:
        return sizeof(uint64_t);
    case WEIVEL_STANDARD_DESCRIBED_I64:
        return sizeof(int64_t);
    case WEIVEL_STANDARD_DESCRIBED_F64:
        return sizeof(double);
    case WEIVEL_STANDARD_DESCRIBED_SIZE:
        return sizeof(size_t);
    case WEIVEL_STANDARD_DESCRIBED_STRUCT:
        type = type_named(state->description, member->type);
        return type == NULL ? 0 : type->size;
    case WEIVEL_STANDARD_DESCRIBED_VALUE:
        return sizeof(weivel_standard_value);
    default:
        return sizeof(void *);
    }
}

static int check_member_size(checking *state, const described_member *member, const field_path *parent) {
    field_path size = {parent, "size", 0};
    return member->size == size_of_kind(state, member) ||
           refuse_invalid(state, &size, "%s is not the size a member of its kind has",
                          "set %s to sizeof the member, as the header declares it");
}

static int check_member_bounds(checking *state, const described_type *type, const described_member *member,
                               const field_path *parent) {
    field_path offset = {parent, "offset", 0};
    return (member->offset <= type->size && member->size <= type->size - member->offset) ||
           refuse_invalid(state, &offset, "%s puts the member past the end of its struct",
                          "set %s to offsetof the member, and the struct's size to its sizeof");
}

static int check_member(checking *state, const described_type *type, const described_member *member,
                        const field_path *path) {
    reference slot = {member->kind, member->element, member->type, member->count, member->release, member_kind, type};
    return check_reference(state, &slot, path) && check_member_size(state, member, path) &&
           check_member_bounds(state, type, member, path);
}

static int check_type(checking *state, const described_type *type, const field_path *parent) {
    field_path first_shape = {parent, "first_shape", 0};
    field_path members = {parent, "members", 0};
    size_t at;
    if (type->first_shape > type->size) {
        return refuse_invalid(state, &first_shape, "%s lies past the end of its struct",
                              "set %s to the offset just past the field the first shape ends at");
    }
    for (at = 0; at < type->member_count; at++) {
        field_path item = {&members, NULL, at};
        if (!check_member(state, type, &type->members[at], &item)) {
            return 0;
        }
    }
    return 1;
}

static int is_published_role(uint32_t role) {
    return role > WEIVEL_STANDARD_DESCRIBED_NO_ROLE && role <= WEIVEL_STANDARD_DESCRIBED_ROLE_LAST;
}

static int is_published_return(uint32_t returns) {
    return returns > WEIVEL_STANDARD_DESCRIBED_NO_RETURN && returns <= WEIVEL_STANDARD_DESCRIBED_RETURN_LAST;
}

static int check_parameter(checking *state, const described_operation *operation,
                           const described_parameter *parameter, const field_path *path) {
    field_path role = {path, "role", 0};
    reference slot = {parameter->kind,    parameter->element, parameter->type, parameter->count,
                      parameter->release, parameter_kind,     operation};
    field_path kind = {path, "kind", 0};
    if (!check_reference(state, &slot, path)) {
        return 0;
    }
    if (!is_published_role(parameter->role)) {
        return refuse_invalid(state, &role, "%s is not a role weivel/description.h publishes",
                              "set %s to INPUT, ANSWER or REFUSAL");
    }
    return parameter->kind != WEIVEL_STANDARD_DESCRIBED_VALUE || parameter->role != WEIVEL_STANDARD_DESCRIBED_INPUT ||
           refuse_invalid(state, &kind, "%s is VALUE on an input, which only an answer tree is",
                          "describe an input value as a POINTER of VALUE in %s");
}

static int check_operation(checking *state, const described_type *definition, const described_operation *operation,
                           const field_path *parent) {
    field_path returns = {parent, "returns", 0};
    field_path offset = {parent, "offset", 0};
    field_path parameters = {parent, "parameters", 0};
    size_t at;
    if (!is_published_return(operation->returns)) {
        return refuse_invalid(state, &returns, "%s is not a return weivel/description.h publishes",
                              "set %s to STATUS or NOTHING");
    }
    if (operation->offset > definition->size || sizeof(void (*)(void)) > definition->size - operation->offset) {
        return refuse_invalid(state, &offset, "%s puts the operation past the end of the definition's struct",
                              "set %s to offsetof the operation's function pointer in the definition");
    }
    for (at = 0; at < operation->parameter_count; at++) {
        field_path item = {&parameters, NULL, at};
        if (!check_parameter(state, operation, &operation->parameters[at], &item)) {
            return 0;
        }
    }
    return 1;
}

static int check_described(checking *state, const weivel_standard_description *description) {
    field_path definition_path = {NULL, "definition", 0};
    field_path types = {NULL, "types", 0};
    field_path operations = {NULL, "operations", 0};
    const described_type *definition;
    size_t at;
    if (!names_everything(state, description)) {
        return 0;
    }
    definition = type_named(description, description->definition);
    if (definition == NULL) {
        return refuse_invalid(state, &definition_path, "%s names a struct the description does not hold",
                              "add the definition's struct to the description's types");
    }
    for (at = 0; at < description->type_count; at++) {
        field_path item = {&types, NULL, at};
        if (!check_type(state, &description->types[at], &item)) {
            return 0;
        }
    }
    for (at = 0; at < description->operation_count; at++) {
        field_path item = {&operations, NULL, at};
        if (!check_operation(state, definition, &description->operations[at], &item)) {
            return 0;
        }
    }
    return 1;
}

weivel_status weivel_standard_description_check(const weivel_standard_description *description,
                                                 weivel_standard_refusal *refusal) {
    weivel_standard_description own;
    checking state;
    if (weivel_standard_refusal_is_unusable(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    if (description == NULL) {
        return weivel_standard_refuse_call(refusal, "weivel_standard_description_check was given no description",
                                           "pass the description the library's description symbol answers");
    }
    if (description->size < DESCRIPTION_FIRST_SHAPE) {
        return weivel_standard_refuse_call(refusal,
                                           "weivel_standard_description_check was given a description smaller than "
                                           "its first shape",
                                           "set the description's size to its sizeof, as the library compiled it");
    }
    weivel_standard_sized_read(&own, sizeof own, description);
    state.description = &own;
    state.refusal = refusal;
    state.status = WEIVEL_STATUS_OK;
    check_described(&state, &own);
    return state.status;
}
