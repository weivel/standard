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

/**
 * `root` is where the description checked lies in the one the caller passed, NULL for that one itself; `given` is the
 * pointer it was reached by and `enclosing` the check of the description holding it, NULL at the top.
 */
typedef struct checking {
    const weivel_standard_description *description;
    weivel_standard_refusal *refusal;
    weivel_status status;
    const weivel_standard_field_path *root;
    unsigned depth;
    const weivel_standard_description *given;
    const struct checking *enclosing;
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

static int names_blocking(checking *state, const weivel_standard_description *description) {
    field_path list = {state->root, "blocking", 0};
    size_t at;
    size_t before;
    if (!require_array(state, description->blocking, description->blocking_count, state->root, "blocking")) {
        return 0;
    }
    for (at = 0; at < description->blocking_count; at++) {
        field_path item = {&list, NULL, at};
        if (description->blocking[at] == NULL) {
            return refuse(state, WEIVEL_STANDARD_REFUSAL_MISSING_FIELD, &item,
                          "%s is NULL, and a description requires it", "set %s to the name of an operation");
        }
        for (before = 0; before < at; before++) {
            if (strcmp(description->blocking[before], description->blocking[at]) == 0) {
                return refuse(state, WEIVEL_STANDARD_REFUSAL_DUPLICATE, &item, "%s repeats a name listed before it",
                              "name each blocking operation once, so %s is listed once");
            }
        }
    }
    return 1;
}

/** A nested description read up to the size it declares, or 0, refused, when it is NULL or below its first shape. */
static int reads_nested(checking *state, const weivel_standard_description *nested, const field_path *path,
                        weivel_standard_description *own) {
    field_path size = {path, "size", 0};
    if (nested == NULL) {
        return refuse(state, WEIVEL_STANDARD_REFUSAL_MISSING_FIELD, path, "%s is NULL, and a description requires it",
                      "point %s at the description of the definition it describes");
    }
    if (nested->size < DESCRIPTION_FIRST_SHAPE) {
        return refuse_invalid(state, &size, "%s is below the description's first shape",
                              "set %s to sizeof the description, as the library compiled it");
    }
    weivel_standard_sized_read(own, sizeof *own, nested);
    return 1;
}

/** Every nested description is set and names its definition, no two the same, so a DEFINITION finds one by name. */
static int names_definitions(checking *state, const weivel_standard_description *description) {
    field_path list = {state->root, "definitions", 0};
    weivel_standard_description own;
    weivel_standard_description other;
    size_t at;
    size_t before;
    if (!require_array(state, description->definitions, description->definition_count, state->root,
                       "definitions")) {
        return 0;
    }
    for (at = 0; at < description->definition_count; at++) {
        field_path item = {&list, NULL, at};
        field_path name = {&item, "definition", 0};
        if (!reads_nested(state, description->definitions[at], &item, &own) ||
            !require_text(state, own.definition, &item, "definition")) {
            return 0;
        }
        for (before = 0; before < at; before++) {
            weivel_standard_sized_read(&other, sizeof other, description->definitions[before]);
            if (strcmp(other.definition, own.definition) == 0) {
                return refuse(state, WEIVEL_STANDARD_REFUSAL_DUPLICATE, &name,
                              "%s repeats the definition of a description listed before it",
                              "describe each definition once, so %s names one description");
            }
        }
    }
    return 1;
}

/** Every name and array the description requires is set, so every later lookup may compare names. */
static int names_everything(checking *state, const weivel_standard_description *description) {
    field_path types = {state->root, "types", 0};
    field_path enums = {state->root, "enums", 0};
    field_path operations = {state->root, "operations", 0};
    size_t at;
    if (!require_text(state, description->definition, state->root, "definition") ||
        !require_array(state, description->types, description->type_count, state->root, "types") ||
        !require_array(state, description->enums, description->enum_count, state->root, "enums") ||
        !require_array(state, description->operations, description->operation_count, state->root, "operations")) {
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
    return names_once(state, description->types, sizeof *description->types, description->type_count, state->root,
                      "types") &&
           names_once(state, description->enums, sizeof *description->enums, description->enum_count, state->root,
                      "enums") &&
           names_once(state, description->operations, sizeof *description->operations, description->operation_count,
                      state->root, "operations") &&
           names_blocking(state, description) && names_definitions(state, description);
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

static int defines_a_nested(const weivel_standard_description *description, const char *name) {
    weivel_standard_description own;
    size_t at;
    for (at = 0; at < description->definition_count; at++) {
        weivel_standard_sized_read(&own, sizeof own, description->definitions[at]);
        if (strcmp(own.definition, name) == 0) {
            return 1;
        }
    }
    return 0;
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
    if (slot->kind == WEIVEL_STANDARD_DESCRIBED_DEFINITION) {
        if (!require_text(state, slot->type, parent, "type")) {
            return 0;
        }
        return defines_a_nested(state->description, slot->type) ||
               refuse_invalid(state, &type, "%s names a definition no description of `definitions` defines",
                              "add the description of the definition %s names to the description's definitions");
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

/** A STRUCT member as large as its struct, the only kind of member a struct holding itself by value can lie on. */
static int fills_its_struct(const described_type *type, const described_member *member) {
    return member->kind == WEIVEL_STANDARD_DESCRIBED_STRUCT && member->size == type->size;
}

/** The member filling `type`, NULL for none; holds_no_second_filler leaves at most one. */
static const described_member *filler_of(const described_type *type) {
    size_t at;
    for (at = 0; at < type->member_count; at++) {
        if (fills_its_struct(type, &type->members[at])) {
            return &type->members[at];
        }
    }
    return NULL;
}

/** No struct is filled by two STRUCT members, which would lie over each other as no C struct's members do. */
static int holds_no_second_filler(checking *state, const weivel_standard_description *description) {
    field_path types = {state->root, "types", 0};
    size_t type_at;
    size_t member_at;
    for (type_at = 0; type_at < description->type_count; type_at++) {
        const described_type *type = &description->types[type_at];
        const described_member *first = filler_of(type);
        field_path item = {&types, NULL, type_at};
        field_path members = {&item, "members", 0};
        for (member_at = 0; first != NULL && member_at < type->member_count; member_at++) {
            field_path member = {&members, NULL, member_at};
            field_path offset = {&member, "offset", 0};
            if (&type->members[member_at] != first && fills_its_struct(type, &type->members[member_at])) {
                return refuse_invalid(state, &offset,
                                      "%s lays a second STRUCT member over the whole of its struct, where no C "
                                      "struct lays two members over each other",
                                      "set %s to offsetof the member, and its size to its sizeof");
            }
        }
    }
    return 1;
}

/**
 * Whether the struct `filler` names, followed through each struct's one filling member, comes back to `holder`.
 *
 * @implNote Sound and complete for a cycle of structs held by value: check_type has made every STRUCT member as
 * large as the struct it names and lie inside its holder, so a struct is never smaller than one it holds, sizes are
 * equal all round a cycle, and every member on the cycle fills its holder; holds_no_second_filler leaves each struct
 * at most one such member, so the walk is a chain. The chain may run into a cycle that misses `holder`, so it stops
 * after the description's type count of steps, each scanning one struct's members: a walk costs at most every
 * member once, and the whole check at most the type count times that.
 */
static int comes_back_to(const weivel_standard_description *description, const described_type *holder,
                         const described_member *filler) {
    const described_type *at = type_named(description, filler->type);
    size_t steps;
    for (steps = 0; at != NULL && steps < description->type_count; steps++) {
        const described_member *next;
        if (at == holder) {
            return 1;
        }
        next = filler_of(at);
        at = next == NULL ? NULL : type_named(description, next->type);
    }
    return 0;
}

/** No struct holds itself by value, directly or through others, which would leave it no finite size. */
static int holds_nothing_inside_itself(checking *state, const weivel_standard_description *description) {
    field_path types = {state->root, "types", 0};
    size_t type_at;
    if (!holds_no_second_filler(state, description)) {
        return 0;
    }
    for (type_at = 0; type_at < description->type_count; type_at++) {
        const described_type *type = &description->types[type_at];
        const described_member *filler = filler_of(type);
        field_path item = {&types, NULL, type_at};
        field_path members = {&item, "members", 0};
        field_path held = {&members, NULL, filler == NULL ? 0 : (size_t)(filler - type->members)};
        field_path named = {&held, "type", 0};
        if (filler != NULL && comes_back_to(description, type, filler)) {
            return refuse_invalid(state, &named,
                                  "%s names a struct that is, or holds by value, the struct holding it, so no "
                                  "struct of that cycle has a finite size",
                                  "point at the struct %s names through a POINTER or an ARRAY, as a struct "
                                  "reaching itself must");
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

static int is_scalar(uint32_t kind) {
    return kind >= WEIVEL_STANDARD_DESCRIBED_U32 && kind <= WEIVEL_STANDARD_DESCRIBED_SIZE;
}

static int reaches_an_element(uint32_t kind) {
    return kind == WEIVEL_STANDARD_DESCRIBED_POINTER || kind == WEIVEL_STANDARD_DESCRIBED_ARRAY;
}

static int may_name_a_type(const reference *slot) {
    return names_a_struct(slot) || slot->kind == WEIVEL_STANDARD_DESCRIBED_U32 ||
           slot->kind == WEIVEL_STANDARD_DESCRIBED_HANDLE || slot->kind == WEIVEL_STANDARD_DESCRIBED_DEFINITION;
}

static int refuse_unused(checking *state, const field_path *parent, const char *key, const char *fix) {
    field_path path = {parent, key, 0};
    return refuse_invalid(state, &path, "%s is set, and its kind leaves it unused", fix);
}

/** Every field the header says is NULL, or NONE, for a slot's kind is. `may_release` is whether it may name one. */
static int leaves_unused_fields_empty(checking *state, const reference *slot, int may_release,
                                      const field_path *parent) {
    if (!reaches_an_element(slot->kind) && slot->element != WEIVEL_STANDARD_DESCRIBED_NONE) {
        return refuse_unused(state, parent, "element", "set %s to NONE: only a POINTER or an ARRAY reaches one");
    }
    if (!may_name_a_type(slot) && slot->type != NULL) {
        return refuse_unused(state, parent, "type",
                             "set %s to NULL: only a struct, a U32 holding an enum, a handle or a definition names a "
                             "type");
    }
    if (!is_counted(slot->kind) && slot->count != NULL) {
        return refuse_unused(state, parent, "count", "set %s to NULL: only texts, bytes or an array is counted");
    }
    if (slot->kind == WEIVEL_STANDARD_DESCRIBED_DEFINITION && slot->release != NULL) {
        return refuse_unused(state, parent, "release",
                             "set %s to NULL: a definition lives as long as what carried it, so nothing frees it");
    }
    return may_release || slot->release == NULL ||
           refuse_unused(state, parent, "release",
                         "set %s to NULL: only a handle, an answer or a refusal is freed by a release");
}

/** A release named anywhere is one: an operation answering NOTHING, never one answering a status. */
static int names_a_release(checking *state, const char *release, const field_path *parent) {
    field_path path = {parent, "release", 0};
    return release == NULL ||
           operation_named(state->description, release)->returns == WEIVEL_STANDARD_DESCRIBED_NOTHING ||
           refuse_invalid(state, &path, "%s names an operation answering a status, where a release answers nothing",
                          "name in %s the release that frees it, an operation answering NOTHING");
}

static int ranges_overlap(size_t first, size_t first_size, size_t second, size_t second_size) {
    return first < second + second_size && second < first + first_size;
}

/** The definition's one context, read from every provider, so it lies where every provider's struct reaches. */
static int places_the_context(checking *state, const described_type *type, int is_definition,
                              const described_member *member, size_t *contexts, const field_path *parent) {
    field_path kind = {parent, "kind", 0};
    field_path offset = {parent, "offset", 0};
    if (member->kind != WEIVEL_STANDARD_DESCRIBED_CONTEXT) {
        return 1;
    }
    if (!is_definition) {
        return refuse_invalid(state, &kind, "%s is CONTEXT outside the definition's struct, which alone holds one",
                              "describe %s by the kind of what it holds, or move the context to the definition");
    }
    if (++*contexts > 1) {
        return refuse_invalid(state, &kind, "%s is a second CONTEXT, where the definition holds one",
                              "describe the definition's one context once, and %s by the kind of what it holds");
    }
    return member->offset + member->size <= type->first_shape ||
           refuse_invalid(state, &offset, "%s puts the context past the definition's first shape",
                          "declare the context, which %s places, inside the definition's first shape");
}

static int type_follows_convention(checking *state, const described_type *type, int is_definition,
                                   const field_path *parent) {
    field_path members = {parent, "members", 0};
    size_t contexts = 0;
    size_t at;
    for (at = 0; at < type->member_count; at++) {
        const described_member *member = &type->members[at];
        field_path item = {&members, NULL, at};
        reference slot = {member->kind, member->element, member->type, member->count, member->release, NULL, NULL};
        if (!leaves_unused_fields_empty(state, &slot, member->kind == WEIVEL_STANDARD_DESCRIBED_HANDLE, &item) ||
            !names_a_release(state, member->release, &item) ||
            !places_the_context(state, type, is_definition, member, &contexts, &item)) {
            return 0;
        }
    }
    return 1;
}

/** No operation shares a name with a data member, or a byte with a member or another operation. */
static int lays_out_the_definition(checking *state, const weivel_standard_description *description,
                                   const described_type *definition) {
    field_path operations = {state->root, "operations", 0};
    const size_t pointer_size = sizeof(void (*)(void));
    size_t at;
    size_t other;
    for (at = 0; at < description->operation_count; at++) {
        const described_operation *operation = &description->operations[at];
        field_path item = {&operations, NULL, at};
        field_path name = {&item, "name", 0};
        field_path offset = {&item, "offset", 0};
        for (other = 0; other < definition->member_count; other++) {
            const described_member *member = &definition->members[other];
            if (strcmp(member->name, operation->name) == 0) {
                return refuse(state, WEIVEL_STANDARD_REFUSAL_DUPLICATE, &name,
                              "%s repeats the name of a member of the definition's struct",
                              "give %s a name no member of the definition's struct has");
            }
            if (ranges_overlap(operation->offset, pointer_size, member->offset, member->size)) {
                return refuse_invalid(state, &offset, "%s overlaps a data member of the definition's struct",
                                      "set %s to offsetof the operation's own function pointer");
            }
        }
        for (other = 0; other < at; other++) {
            if (ranges_overlap(operation->offset, pointer_size, description->operations[other].offset, pointer_size)) {
                return refuse_invalid(state, &offset, "%s overlaps an operation listed before it",
                                      "set %s to offsetof the operation's own function pointer");
            }
        }
    }
    return 1;
}

static int is_input_kind(uint32_t kind) {
    return kind == WEIVEL_STANDARD_DESCRIBED_CONTEXT || is_scalar(kind) || kind == WEIVEL_STANDARD_DESCRIBED_TEXT ||
           is_counted(kind) || kind == WEIVEL_STANDARD_DESCRIBED_POINTER || kind == WEIVEL_STANDARD_DESCRIBED_HANDLE;
}

static int is_answer_kind(uint32_t kind) {
    return is_scalar(kind) || kind == WEIVEL_STANDARD_DESCRIBED_TEXT || kind == WEIVEL_STANDARD_DESCRIBED_POINTER ||
           kind == WEIVEL_STANDARD_DESCRIBED_VALUE || kind == WEIVEL_STANDARD_DESCRIBED_HANDLE ||
           kind == WEIVEL_STANDARD_DESCRIBED_DEFINITION;
}

/** The kinds each role takes, as weivel/description.h passes each kind as a parameter. */
static int takes_its_kind_in_its_role(checking *state, const described_parameter *parameter,
                                      const field_path *parent) {
    field_path kind = {parent, "kind", 0};
    field_path element = {parent, "element", 0};
    if (parameter->role == WEIVEL_STANDARD_DESCRIBED_INPUT) {
        return is_input_kind(parameter->kind) ||
               refuse_invalid(state, &kind, "%s is a kind an input does not take",
                              "describe the input in %s as the context, a scalar, a text, texts, bytes, a pointer, "
                              "an array or a handle");
    }
    if (parameter->role == WEIVEL_STANDARD_DESCRIBED_ANSWER) {
        if (!is_answer_kind(parameter->kind)) {
            return refuse_invalid(state, &kind, "%s is a kind an answer does not take",
                                  "describe the answer in %s as a scalar, a text, a pointer to a struct, a value "
                                  "tree, a handle or a definition");
        }
        return parameter->kind != WEIVEL_STANDARD_DESCRIBED_POINTER ||
               parameter->element == WEIVEL_STANDARD_DESCRIBED_STRUCT ||
               refuse_invalid(state, &element, "%s is VALUE on a pointer answer, where an answer tree is a VALUE",
                              "describe the answer as a VALUE, and %s as NONE");
    }
    return (parameter->kind == WEIVEL_STANDARD_DESCRIBED_POINTER &&
            parameter->element == WEIVEL_STANDARD_DESCRIBED_STRUCT) ||
           refuse_invalid(state, &kind, "%s is not a POINTER of STRUCT, the one form a refusal takes",
                          "describe the refusal in %s as a POINTER of STRUCT");
}

static int may_name_a_release(const described_parameter *parameter) {
    return parameter->kind == WEIVEL_STANDARD_DESCRIBED_HANDLE || parameter->role != WEIVEL_STANDARD_DESCRIBED_INPUT;
}

/** A scalar's or a text's answer is the provider's own value, so no release is handed anything of it. */
static int frees_only_what_owns_storage(checking *state, const described_parameter *parameter,
                                        const field_path *parent) {
    field_path release = {parent, "release", 0};
    int owns_nothing = is_scalar(parameter->kind) || parameter->kind == WEIVEL_STANDARD_DESCRIBED_TEXT;
    return parameter->role != WEIVEL_STANDARD_DESCRIBED_ANSWER || !owns_nothing || parameter->release == NULL ||
           refuse_invalid(state, &release, "%s names a release for a scalar or a text answer, which owns nothing",
                          "set %s to NULL");
}

static int carries_a_text(const described_type *type, const char *name) {
    size_t at;
    for (at = 0; at < type->member_count; at++) {
        if (strcmp(type->members[at].name, name) == 0) {
            return type->members[at].kind == WEIVEL_STANDARD_DESCRIBED_TEXT;
        }
    }
    return 0;
}

static int refuses_with_texts(checking *state, const described_parameter *parameter, const field_path *parent) {
    field_path type_path = {parent, "type", 0};
    const described_type *type;
    if (parameter->role != WEIVEL_STANDARD_DESCRIBED_REFUSAL) {
        return 1;
    }
    type = type_named(state->description, parameter->type);
    if (!carries_a_text(type, "detail")) {
        return refuse_invalid(state, &type_path, "%s names a struct carrying no TEXT member named detail",
                              "name in %s a refusal struct carrying `detail` and `fix` texts");
    }
    return carries_a_text(type, "fix") ||
           refuse_invalid(state, &type_path, "%s names a struct carrying no TEXT member named fix",
                          "name in %s a refusal struct carrying `detail` and `fix` texts");
}

static int parameter_follows_convention(checking *state, const described_parameter *parameter, size_t at,
                                        const field_path *parent) {
    field_path kind = {parent, "kind", 0};
    reference slot = {parameter->kind, parameter->element, parameter->type, parameter->count, parameter->release,
                      NULL, NULL};
    if (at > 0 && parameter->kind == WEIVEL_STANDARD_DESCRIBED_CONTEXT) {
        return refuse_invalid(state, &kind, "%s is CONTEXT past the first parameter, the one the context is",
                              "describe %s by the kind of what it is");
    }
    return takes_its_kind_in_its_role(state, parameter, parent) &&
           leaves_unused_fields_empty(state, &slot, may_name_a_release(parameter), parent) &&
           names_a_release(state, parameter->release, parent) &&
           frees_only_what_owns_storage(state, parameter, parent) && refuses_with_texts(state, parameter, parent);
}

static int takes_the_context_first(checking *state, const described_operation *operation, const field_path *parent) {
    field_path parameters = {parent, "parameters", 0};
    field_path first = {&parameters, NULL, 0};
    field_path kind = {&first, "kind", 0};
    if (operation->parameter_count == 0) {
        return refuse_invalid(state, &parameters, "%s is empty, where every operation takes the context first",
                              "describe the context as the first of %s");
    }
    return operation->parameters[0].kind == WEIVEL_STANDARD_DESCRIBED_CONTEXT ||
           refuse_invalid(state, &kind, "%s is not CONTEXT, which every operation takes first",
                          "describe the context as the operation's first parameter, so %s is CONTEXT");
}

/** A status operation's refusal, when it takes one, is its last parameter, and it takes no second. */
static int refuses_last(checking *state, const described_operation *operation, const field_path *parent) {
    field_path parameters = {parent, "parameters", 0};
    size_t at;
    for (at = 0; at + 1 < operation->parameter_count; at++) {
        field_path item = {&parameters, NULL, at};
        field_path role = {&item, "role", 0};
        if (operation->parameters[at].role == WEIVEL_STANDARD_DESCRIBED_REFUSAL) {
            return refuse_invalid(state, &role, "%s is a REFUSAL before the operation's last parameter",
                                  "take the one refusal last, so no parameter follows %s");
        }
    }
    return 1;
}

static int frees_one_thing(checking *state, const described_operation *operation, const field_path *parent) {
    field_path parameters = {parent, "parameters", 0};
    return operation->parameter_count == 2 ||
           refuse_invalid(state, &parameters,
                          "%s is not two parameters, where a release takes the context and the one thing it frees",
                          "describe %s as the context and the one thing the release frees");
}

/** A definition lives as long as what carried it, so no release is handed one. */
static int frees_no_definition(checking *state, const described_operation *operation, const field_path *parent) {
    field_path parameters = {parent, "parameters", 0};
    size_t at;
    for (at = 0; at < operation->parameter_count; at++) {
        field_path item = {&parameters, NULL, at};
        field_path kind = {&item, "kind", 0};
        if (operation->parameters[at].kind == WEIVEL_STANDARD_DESCRIBED_DEFINITION) {
            return refuse_invalid(state, &kind, "%s is DEFINITION in a release, which frees nothing a definition is",
                                  "describe the release in %s as freeing an answer, a refusal or a handle");
        }
    }
    return 1;
}

static int operation_follows_convention(checking *state, const described_operation *operation,
                                        const field_path *parent) {
    field_path parameters = {parent, "parameters", 0};
    size_t at;
    if (!takes_the_context_first(state, operation, parent)) {
        return 0;
    }
    if (operation->returns == WEIVEL_STANDARD_DESCRIBED_STATUS) {
        if (!refuses_last(state, operation, parent)) {
            return 0;
        }
    } else if (!frees_one_thing(state, operation, parent) || !frees_no_definition(state, operation, parent)) {
        return 0;
    }
    for (at = 0; at < operation->parameter_count; at++) {
        field_path item = {&parameters, NULL, at};
        if (!parameter_follows_convention(state, &operation->parameters[at], at, &item)) {
            return 0;
        }
    }
    return 1;
}

/**
 * The standard's convention for a definition, which every binding builds on, checked once a description has passed
 * every rule of its form, so each fault here is a sound description shaped against the convention.
 */
static int follows_the_convention(checking *state, const weivel_standard_description *description,
                                  const described_type *definition) {
    field_path types = {state->root, "types", 0};
    field_path operations = {state->root, "operations", 0};
    size_t at;
    for (at = 0; at < description->type_count; at++) {
        field_path item = {&types, NULL, at};
        if (!type_follows_convention(state, &description->types[at], &description->types[at] == definition,
                                     &item)) {
            return 0;
        }
    }
    if (!lays_out_the_definition(state, description, definition)) {
        return 0;
    }
    for (at = 0; at < description->operation_count; at++) {
        field_path item = {&operations, NULL, at};
        if (!operation_follows_convention(state, &description->operations[at], &item)) {
            return 0;
        }
    }
    return 1;
}

/** A blocking name is one of the description's operations, never a release, which answers nothing and never waits. */
static int blocks_only_in_operations(checking *state, const weivel_standard_description *description) {
    field_path list = {state->root, "blocking", 0};
    size_t at;
    for (at = 0; at < description->blocking_count; at++) {
        field_path item = {&list, NULL, at};
        const described_operation *operation = operation_named(description, description->blocking[at]);
        if (operation == NULL) {
            return refuse_invalid(state, &item, "%s names an operation the description does not hold",
                                  "name in %s an operation as the description's operations list it");
        }
        if (operation->returns != WEIVEL_STANDARD_DESCRIBED_STATUS) {
            return refuse_invalid(state, &item, "%s names a release, which never blocks",
                                  "name in %s an operation answering a status, or take the release out of it");
        }
    }
    return 1;
}

static int check_described(checking *state, const weivel_standard_description *description);

static int encloses(const checking *state, const weivel_standard_description *nested) {
    const checking *at;
    for (at = state; at != NULL; at = at->enclosing) {
        if (at->given == nested) {
            return 1;
        }
    }
    return 0;
}

/**
 * Each nested description passes the whole check itself. One holding a description it is held by is refused, and a
 * depth bound refuses a chain too long to bind, a cycle through copies included.
 */
static int checks_every_nested(checking *state, const weivel_standard_description *description) {
    field_path list = {state->root, "definitions", 0};
    size_t at;
    for (at = 0; at < description->definition_count; at++) {
        field_path item = {&list, NULL, at};
        weivel_standard_description own;
        checking nested;
        if (encloses(state, description->definitions[at])) {
            return refuse_invalid(state, &item, "%s is a description that holds it, so it nests itself",
                                  "point %s at the description of another definition, never at one holding it");
        }
        if (state->depth >= WEIVEL_STANDARD_DESCRIPTION_DEPTH_LIMIT) {
            return refuse_invalid(state, &item, "%s nests deeper than WEIVEL_STANDARD_DESCRIPTION_DEPTH_LIMIT",
                                  "nest %s no deeper than the limit");
        }
        weivel_standard_sized_read(&own, sizeof own, description->definitions[at]);
        nested.description = &own;
        nested.refusal = state->refusal;
        nested.status = WEIVEL_STATUS_OK;
        nested.root = &item;
        nested.depth = state->depth + 1;
        nested.given = description->definitions[at];
        nested.enclosing = state;
        if (!check_described(&nested, &own)) {
            state->status = nested.status;
            return 0;
        }
    }
    return 1;
}

static int check_described(checking *state, const weivel_standard_description *description) {
    field_path definition_path = {state->root, "definition", 0};
    field_path types = {state->root, "types", 0};
    field_path operations = {state->root, "operations", 0};
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
    if (!holds_nothing_inside_itself(state, description)) {
        return 0;
    }
    for (at = 0; at < description->operation_count; at++) {
        field_path item = {&operations, NULL, at};
        if (!check_operation(state, definition, &description->operations[at], &item)) {
            return 0;
        }
    }
    return blocks_only_in_operations(state, description) && follows_the_convention(state, description, definition) &&
           checks_every_nested(state, description);
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
    state.root = NULL;
    state.depth = 0;
    state.given = description;
    state.enclosing = NULL;
    check_described(&state, &own);
    return state.status;
}
