#include "model.h"

#include <string.h>

#include "allocation.h"
#include "tokens.h"

describe_names describe_names_of_the_standard(void) {
    describe_names names;
    names.status = "weivel_status";
    names.refusal = "weivel_standard_service_refusal";
    names.value = "weivel_standard_value";
    names.tree = "weivel_standard_value_tree";
    return names;
}

static void release_item(describe_item *item) {
    weivel_standard_memory_release(item->name);
    weivel_standard_memory_release(item->spelling.base);
    weivel_standard_memory_release(item->doc);
    weivel_standard_memory_release(item->type);
    weivel_standard_memory_release(item->count);
    weivel_standard_memory_release(item->release);
}

static void release_operation(describe_operation *operation) {
    size_t at;
    for (at = 0; at < operation->parameter_count; at++) {
        release_item(&operation->parameters[at]);
    }
    weivel_standard_memory_release(operation->parameters);
    weivel_standard_memory_release(operation->name);
    weivel_standard_memory_release(operation->doc);
    weivel_standard_memory_release(operation->returned.base);
    weivel_standard_memory_release(operation->frees);
}

static void release_type(describe_type *type) {
    size_t at;
    for (at = 0; at < type->member_count; at++) {
        release_item(&type->members[at]);
    }
    for (at = 0; at < type->operation_count; at++) {
        release_operation(&type->operations[at]);
    }
    weivel_standard_memory_release(type->members);
    weivel_standard_memory_release(type->operations);
    weivel_standard_memory_release(type->name);
    weivel_standard_memory_release(type->doc);
    weivel_standard_memory_release(type->first_shape);
}

static void release_enum(describe_enum *described) {
    size_t at;
    for (at = 0; at < described->entry_count; at++) {
        weivel_standard_memory_release(described->entries[at].name);
    }
    weivel_standard_memory_release(described->entries);
    weivel_standard_memory_release(described->name);
    weivel_standard_memory_release(described->doc);
}

void describe_model_release(describe_model *model) {
    size_t at;
    for (at = 0; at < model->type_count; at++) {
        release_type(&model->types[at]);
    }
    for (at = 0; at < model->enum_count; at++) {
        release_enum(&model->enums[at]);
    }
    for (at = 0; at < model->handle_count; at++) {
        weivel_standard_memory_release(model->handles[at].name);
        weivel_standard_memory_release(model->handles[at].release);
    }
    for (at = 0; at < model->file_count; at++) {
        weivel_standard_memory_release(model->files[at]);
    }
    weivel_standard_memory_release(model->types);
    weivel_standard_memory_release(model->enums);
    weivel_standard_memory_release(model->handles);
    weivel_standard_memory_release(model->files);
    memset(model, 0, sizeof *model);
}

void describe_refusal_release(describe_refusal *refusal) {
    weivel_standard_memory_release(refusal->file);
    weivel_standard_memory_release(refusal->message);
    memset(refusal, 0, sizeof *refusal);
}

const describe_type *describe_model_type(const describe_model *model, const char *name) {
    size_t at;
    for (at = 0; at < model->type_count; at++) {
        if (strcmp(model->types[at].name, name) == 0) {
            return &model->types[at];
        }
    }
    return NULL;
}

const describe_enum *describe_model_enum(const describe_model *model, const char *name) {
    size_t at;
    for (at = 0; at < model->enum_count; at++) {
        if (strcmp(model->enums[at].name, name) == 0) {
            return &model->enums[at];
        }
    }
    return NULL;
}

const describe_handle *describe_model_handle(const describe_model *model, const char *name) {
    size_t at;
    for (at = 0; at < model->handle_count; at++) {
        if (strcmp(model->handles[at].name, name) == 0) {
            return &model->handles[at];
        }
    }
    return NULL;
}

const describe_operation *describe_type_operation(const describe_type *type, const char *name) {
    size_t at;
    for (at = 0; at < type->operation_count; at++) {
        if (strcmp(type->operations[at].name, name) == 0) {
            return &type->operations[at];
        }
    }
    return NULL;
}

static int names_a_struct(const describe_item *item) {
    return item->kind == WEIVEL_STANDARD_DESCRIBED_STRUCT ||
           ((item->kind == WEIVEL_STANDARD_DESCRIBED_POINTER || item->kind == WEIVEL_STANDARD_DESCRIBED_ARRAY) &&
            item->element == WEIVEL_STANDARD_DESCRIBED_STRUCT);
}

typedef struct reaching {
    const describe_model *model;
    const describe_type *definition;
    unsigned char *types;
    unsigned char *enums;
    describe_refusal *refusal;
} reaching;

static describe_outcome reach_items(reaching *state, const describe_item *items, size_t count);

static describe_outcome reach_type(reaching *state, const char *name) {
    const describe_type *type = describe_model_type(state->model, name);
    size_t index;
    if (type == NULL) {
        return DESCRIBE_DONE;
    }
    index = (size_t)(type - state->model->types);
    if (state->types[index]) {
        return DESCRIBE_DONE;
    }
    state->types[index] = 1;
    return reach_items(state, type->members, type->member_count);
}

static int releases_handle(const describe_type *definition, const describe_item *item) {
    const describe_operation *release = describe_type_operation(definition, item->release);
    return release != NULL && release->returns == WEIVEL_STANDARD_DESCRIBED_NOTHING &&
           strcmp(release->frees, item->type) == 0;
}

static describe_outcome reach_items(reaching *state, const describe_item *items, size_t count) {
    size_t at;
    for (at = 0; at < count; at++) {
        const describe_item *item = &items[at];
        describe_outcome outcome = DESCRIBE_DONE;
        if (names_a_struct(item)) {
            outcome = reach_type(state, item->type);
        } else if (item->kind == WEIVEL_STANDARD_DESCRIBED_U32 && item->type != NULL) {
            const describe_enum *named = describe_model_enum(state->model, item->type);
            if (named != NULL) {
                state->enums[named - state->model->enums] = 1;
            }
        } else if (item->kind == WEIVEL_STANDARD_DESCRIBED_HANDLE && !releases_handle(state->definition, item)) {
            outcome = describe_refuse(state->refusal, item->where.file, item->where.line,
                                      "`%s` holds the handle `%s`, which `%s` reaches but holds no release `%s` of",
                                      item->name, item->type, state->definition->name, item->release);
        }
        if (outcome != DESCRIBE_DONE) {
            return outcome;
        }
    }
    return DESCRIBE_DONE;
}

static describe_outcome reach_all(reaching *state) {
    const describe_type *definition = state->definition;
    describe_outcome outcome = reach_type(state, definition->name);
    size_t at;
    for (at = 0; outcome == DESCRIBE_DONE && at < definition->operation_count; at++) {
        outcome = reach_items(state, definition->operations[at].parameters, definition->operations[at].parameter_count);
    }
    return outcome;
}

static int collect(describe_reach *reach, const describe_model *model, const unsigned char *types,
                   const unsigned char *enums) {
    size_t at;
    reach->types = weivel_standard_memory_allocate_zeroed(model->type_count + 1, sizeof *reach->types);
    reach->enums = weivel_standard_memory_allocate_zeroed(model->enum_count + 1, sizeof *reach->enums);
    if (reach->types == NULL || reach->enums == NULL) {
        return 0;
    }
    for (at = 0; at < model->type_count; at++) {
        if (types[at]) {
            reach->types[reach->type_count++] = &model->types[at];
        }
    }
    for (at = 0; at < model->enum_count; at++) {
        if (enums[at]) {
            reach->enums[reach->enum_count++] = &model->enums[at];
        }
    }
    return 1;
}

describe_outcome describe_reach_of(const describe_model *model, const char *definition, describe_reach *reach,
                                   describe_refusal *refusal) {
    reaching state;
    describe_outcome outcome;
    memset(reach, 0, sizeof *reach);
    state.model = model;
    state.definition = describe_model_type(model, definition);
    state.refusal = refusal;
    if (state.definition == NULL || !state.definition->definition) {
        return describe_refuse(refusal, NULL, 0, "`%s` is not a definition any scanned header declares", definition);
    }
    state.types = weivel_standard_memory_allocate_zeroed(model->type_count + 1, 1);
    state.enums = weivel_standard_memory_allocate_zeroed(model->enum_count + 1, 1);
    outcome = state.types == NULL || state.enums == NULL ? DESCRIBE_OUT_OF_MEMORY : reach_all(&state);
    if (outcome == DESCRIBE_DONE && !collect(reach, model, state.types, state.enums)) {
        outcome = DESCRIBE_OUT_OF_MEMORY;
    }
    weivel_standard_memory_release(state.types);
    weivel_standard_memory_release(state.enums);
    if (outcome != DESCRIBE_DONE) {
        describe_reach_release(reach);
        return outcome;
    }
    reach->definition = state.definition;
    return DESCRIBE_DONE;
}

void describe_reach_release(describe_reach *reach) {
    weivel_standard_memory_release((void *)reach->types);
    weivel_standard_memory_release((void *)reach->enums);
    memset(reach, 0, sizeof *reach);
}
