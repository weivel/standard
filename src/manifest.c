#include "weivel/standard.h"

#include "allocation.h"
#include "report.h"
#include "weivel/json.h"

#include <string.h>

#define LARGEST_LIST_LENGTH 4294967295u

typedef weivel_standard_field_path field_path;
typedef weivel_standard_refusal_texts refusal_texts;

struct weivel_standard_manifest {
    weivel_manifest view;
    void **blocks;
    size_t block_count;
    size_t block_capacity;
};

typedef struct reading {
    weivel_standard_manifest *owner;
    weivel_standard_refusal *refusal;
    weivel_status status;
} reading;

typedef int (*element_reader)(reading *state, const weivel_json_value *value, const field_path *path, void *element);

static int refuse(reading *state, uint32_t code, refusal_texts texts) {
    if (state->refusal == NULL) {
        weivel_standard_release_texts(&texts);
        state->status = WEIVEL_STATUS_REFUSED;
        return 0;
    }
    state->status =
        weivel_standard_fill_refusal(state->refusal, code, texts) ? WEIVEL_STATUS_REFUSED : WEIVEL_STATUS_OUT_OF_MEMORY;
    return 0;
}

static int refuse_wrong_type(reading *state, const weivel_json_value *value, const field_path *path,
                             const char *expected) {
    refusal_texts texts = {NULL, NULL, NULL};
    texts.path = weivel_standard_path_text(path);
    if (texts.path != NULL) {
        const char *subject = weivel_standard_subject_of(texts.path);
        texts.detail = weivel_standard_format_text("%s is %s, where %s is needed", subject,
                                                   weivel_standard_kind_phrase(value), expected);
        texts.fix = weivel_standard_format_text("write %s as %s", subject, expected);
    }
    return refuse(state, WEIVEL_STANDARD_REFUSAL_WRONG_TYPE, texts);
}

/** `fix` holds one %s, which names the field. */
static int refuse_unrepresentable(reading *state, const field_path *path, const char *what, const char *fix) {
    refusal_texts texts = {NULL, NULL, NULL};
    texts.path = weivel_standard_path_text(path);
    if (texts.path != NULL) {
        texts.detail = weivel_standard_format_text("%s %s", weivel_standard_subject_of(texts.path), what);
        texts.fix = weivel_standard_format_text(fix, weivel_standard_subject_of(texts.path));
    }
    return refuse(state, WEIVEL_STANDARD_REFUSAL_UNREPRESENTABLE, texts);
}

static int out_of_memory(reading *state) {
    state->status = WEIVEL_STATUS_OUT_OF_MEMORY;
    return 0;
}

static int owner_keep(weivel_standard_manifest *owner, void *block) {
    if (owner->block_count == owner->block_capacity) {
        size_t wanted = owner->block_capacity == 0 ? 16 : owner->block_capacity * 2;
        void **grown = weivel_standard_memory_reallocate(owner->blocks, wanted * sizeof *grown);
        if (grown == NULL) {
            return 0;
        }
        owner->blocks = grown;
        owner->block_capacity = wanted;
    }
    owner->blocks[owner->block_count++] = block;
    return 1;
}

static void *owner_take(reading *state, size_t count, size_t size) {
    void *block = weivel_standard_memory_allocate_zeroed(count, size);
    if (block == NULL || !owner_keep(state->owner, block)) {
        weivel_standard_memory_release(block);
        out_of_memory(state);
        return NULL;
    }
    return block;
}

static int copy_text(reading *state, const char *text, size_t length, const char **copy) {
    char *kept = owner_take(state, length + 1, 1);
    if (kept == NULL) {
        return 0;
    }
    memcpy(kept, text, length);
    *copy = kept;
    return 1;
}

static int read_text(reading *state, const weivel_json_value *value, const field_path *path, const char **text) {
    const char *decoded = weivel_json_as_string(value);
    size_t length = weivel_json_string_length(value);
    if (decoded == NULL) {
        return refuse_wrong_type(state, value, path, "a string");
    }
    if (strlen(decoded) != length) {
        return refuse_unrepresentable(state, path, "holds a NUL character, which the core's strings cannot carry",
                                      "remove the NUL character from %s");
    }
    return copy_text(state, decoded, length, text);
}

static int read_optional_text(reading *state, const weivel_json_value *value, const field_path *path,
                              const char **text) {
    *text = NULL;
    return value == NULL || read_text(state, value, path, text);
}

static int read_text_element(reading *state, const weivel_json_value *value, const field_path *path, void *element) {
    return read_text(state, value, path, (const char **)element);
}

/** Reads an array of `element_size` items, leaving `*items` NULL and `*count` 0 when it is absent or empty. */
static int read_list(reading *state, const weivel_json_value *value, const field_path *path, size_t element_size,
                     element_reader read_element, void **items, uint32_t *count) {
    size_t length;
    size_t at;
    char *storage;
    *items = NULL;
    *count = 0;
    if (value == NULL) {
        return 1;
    }
    if (weivel_json_kind_of(value) != WEIVEL_JSON_ARRAY) {
        return refuse_wrong_type(state, value, path, "an array");
    }
    length = weivel_json_array_length(value);
    if (length > LARGEST_LIST_LENGTH) {
        return refuse_unrepresentable(state, path, "has more than 4294967295 items",
                                      "list at most 4294967295 items in %s");
    }
    if (length == 0) {
        return 1;
    }
    storage = owner_take(state, length, element_size);
    if (storage == NULL) {
        return 0;
    }
    for (at = 0; at < length; at++) {
        field_path item = {path, NULL, at};
        if (!read_element(state, weivel_json_array_at(value, at), &item, storage + at * element_size)) {
            return 0;
        }
    }
    *items = storage;
    *count = (uint32_t)length;
    return 1;
}

static int read_strings(reading *state, const weivel_json_value *value, const field_path *path,
                        weivel_strings *strings) {
    void *items;
    if (!read_list(state, value, path, sizeof(const char *), read_text_element, &items, &strings->count)) {
        return 0;
    }
    strings->items = items;
    return 1;
}

static int read_number(reading *state, const weivel_json_value *value, const field_path *path, weivel_number *number) {
    number->size = (uint32_t)sizeof *number;
    if (!weivel_json_as_double(value, &number->value)) {
        return refuse_wrong_type(state, value, path, "a number");
    }
    return copy_text(state, path->key, strlen(path->key), &number->name);
}

/** Reads an object of named numbers, as `attributes` and `constraints` are written. */
static int read_numbers(reading *state, const weivel_json_value *value, const field_path *path,
                        weivel_numbers *numbers) {
    size_t count;
    size_t at;
    weivel_number *items;
    numbers->items = NULL;
    numbers->count = 0;
    if (value == NULL) {
        return 1;
    }
    if (weivel_json_kind_of(value) != WEIVEL_JSON_OBJECT) {
        return refuse_wrong_type(state, value, path, "an object");
    }
    count = weivel_json_object_size(value);
    if (count == 0) {
        return 1;
    }
    items = owner_take(state, count, sizeof *items);
    if (items == NULL) {
        return 0;
    }
    for (at = 0; at < count; at++) {
        field_path member = {path, weivel_json_object_key_at(value, at), 0};
        if (!read_number(state, weivel_json_object_value_at(value, at), &member, &items[at])) {
            return 0;
        }
    }
    numbers->items = items;
    numbers->count = (uint32_t)count;
    return 1;
}

static int read_choice(reading *state, const weivel_json_value *value, const field_path *path, uint32_t *choice) {
    int flag = 0;
    *choice = WEIVEL_CHOICE_DEFAULT;
    if (value == NULL) {
        return 1;
    }
    if (!weivel_json_as_bool(value, &flag)) {
        return refuse_wrong_type(state, value, path, "a boolean");
    }
    *choice = flag ? WEIVEL_CHOICE_YES : WEIVEL_CHOICE_NO;
    return 1;
}

static int read_api(reading *state, const weivel_json_value *value, const field_path *path, uint32_t *api) {
    double level = 0;
    *api = 0;
    if (value == NULL) {
        return 1;
    }
    if (!weivel_json_as_double(value, &level) || !weivel_standard_is_whole_number(level)) {
        return refuse_wrong_type(state, value, path, "a whole number");
    }
    if (level < 0 || level > 4294967295.0) {
        return refuse_unrepresentable(state, path, "is outside 0 to 4294967295",
                                      "set %s to a whole number from 0 to 4294967295");
    }
    *api = (uint32_t)level;
    return 1;
}

static const weivel_json_value *member_of(const weivel_json_value *object, const char *key) {
    return weivel_json_object_get(object, key);
}

static int read_offer_object(reading *state, const weivel_json_value *value, const field_path *path,
                             weivel_service_offer *offer) {
    field_path id = {path, "id", 0};
    field_path attributes = {path, "attributes", 0};
    return read_optional_text(state, member_of(value, "id"), &id, &offer->id) &&
           read_numbers(state, member_of(value, "attributes"), &attributes, &offer->attributes);
}

static int read_offer(reading *state, const weivel_json_value *value, const field_path *path, void *element) {
    weivel_service_offer *offer = element;
    offer->size = (uint32_t)sizeof *offer;
    switch (weivel_json_kind_of(value)) {
    case WEIVEL_JSON_STRING:
        return read_text(state, value, path, &offer->id);
    case WEIVEL_JSON_OBJECT:
        return read_offer_object(state, value, path, offer);
    default:
        return refuse_wrong_type(state, value, path, "a string or an object");
    }
}

static int read_want_object(reading *state, const weivel_json_value *value, const field_path *path,
                            weivel_service_want *want) {
    field_path id = {path, "id", 0};
    field_path provider = {path, "provider", 0};
    field_path optional = {path, "optional", 0};
    field_path degradable = {path, "degradable", 0};
    field_path constraints = {path, "constraints", 0};
    return read_optional_text(state, member_of(value, "id"), &id, &want->id) &&
           read_optional_text(state, member_of(value, "provider"), &provider, &want->provider) &&
           read_choice(state, member_of(value, "optional"), &optional, &want->optional) &&
           read_choice(state, member_of(value, "degradable"), &degradable, &want->degradable) &&
           read_numbers(state, member_of(value, "constraints"), &constraints, &want->constraints);
}

static int read_want(reading *state, const weivel_json_value *value, const field_path *path, void *element) {
    weivel_service_want *want = element;
    want->size = (uint32_t)sizeof *want;
    switch (weivel_json_kind_of(value)) {
    case WEIVEL_JSON_STRING:
        return read_text(state, value, path, &want->id);
    case WEIVEL_JSON_OBJECT:
        return read_want_object(state, value, path, want);
    default:
        return refuse_wrong_type(state, value, path, "a string or an object");
    }
}

static int read_offers(reading *state, const weivel_json_value *value, const field_path *path,
                       weivel_service_offers *offers) {
    void *items;
    if (!read_list(state, value, path, sizeof(weivel_service_offer), read_offer, &items, &offers->count)) {
        return 0;
    }
    offers->items = items;
    return 1;
}

static int read_wants(reading *state, const weivel_json_value *value, const field_path *path,
                      weivel_service_wants *wants) {
    void *items;
    if (!read_list(state, value, path, sizeof(weivel_service_want), read_want, &items, &wants->count)) {
        return 0;
    }
    wants->items = items;
    return 1;
}

static int is_absent_or_object(reading *state, const weivel_json_value *value, const field_path *path) {
    if (value == NULL || weivel_json_kind_of(value) == WEIVEL_JSON_OBJECT) {
        return 1;
    }
    return refuse_wrong_type(state, value, path, "an object");
}

static int read_capabilities(reading *state, const weivel_json_value *value, const field_path *path,
                             weivel_capability_declarations *capabilities) {
    field_path provides = {path, "provides", 0};
    field_path consumes = {path, "consumes", 0};
    return is_absent_or_object(state, value, path) &&
           read_strings(state, member_of(value, "provides"), &provides, &capabilities->provides) &&
           read_strings(state, member_of(value, "consumes"), &consumes, &capabilities->consumes);
}

static int read_services(reading *state, const weivel_json_value *value, const field_path *path,
                         weivel_service_declarations *services) {
    field_path provides = {path, "provides", 0};
    field_path consumes = {path, "consumes", 0};
    return is_absent_or_object(state, value, path) &&
           read_offers(state, member_of(value, "provides"), &provides, &services->provides) &&
           read_wants(state, member_of(value, "consumes"), &consumes, &services->consumes);
}

static int read_document(reading *state, const weivel_json_value *root, weivel_manifest *view) {
    field_path import_name = {NULL, "importName", 0};
    field_path api = {NULL, "api", 0};
    field_path entry = {NULL, "entry", 0};
    field_path targets = {NULL, "targets", 0};
    field_path capabilities = {NULL, "capabilities", 0};
    field_path services = {NULL, "services", 0};
    field_path permissions = {NULL, "permissions", 0};
    if (weivel_json_kind_of(root) != WEIVEL_JSON_OBJECT) {
        return refuse_wrong_type(state, root, NULL, "an object");
    }
    return read_optional_text(state, member_of(root, "importName"), &import_name, &view->import_name) &&
           read_api(state, member_of(root, "api"), &api, &view->api) &&
           read_optional_text(state, member_of(root, "entry"), &entry, &view->entry) &&
           read_strings(state, member_of(root, "targets"), &targets, &view->targets) &&
           read_capabilities(state, member_of(root, "capabilities"), &capabilities, &view->capabilities) &&
           read_services(state, member_of(root, "services"), &services, &view->services) &&
           read_strings(state, member_of(root, "permissions"), &permissions, &view->permissions);
}

static weivel_status convert(const weivel_json_value *root, weivel_standard_manifest **manifest,
                             weivel_standard_refusal *refusal) {
    reading state;
    weivel_standard_manifest *owner = weivel_standard_memory_allocate_zeroed(1, sizeof *owner);
    if (owner == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    owner->view.size = (uint32_t)sizeof owner->view;
    state.owner = owner;
    state.refusal = refusal;
    state.status = WEIVEL_STATUS_OK;
    if (!read_document(&state, root, &owner->view)) {
        weivel_standard_manifest_release(owner);
        return state.status;
    }
    *manifest = owner;
    return WEIVEL_STATUS_OK;
}

weivel_status weivel_standard_read_manifest(const char *json, size_t length, weivel_standard_manifest **manifest,
                                            weivel_standard_refusal *refusal) {
    weivel_json_error error = {.size = (uint32_t)sizeof(weivel_json_error)};
    weivel_json_value *root;
    weivel_status status;
    if (manifest != NULL) {
        *manifest = NULL;
    }
    if (weivel_standard_refusal_is_unusable(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    if (manifest == NULL) {
        return weivel_standard_refuse_call(refusal,
                                           "weivel_standard_read_manifest was given no place to put the manifest",
                                           "pass the address of a weivel_standard_manifest pointer");
    }
    if (json == NULL) {
        return weivel_standard_refuse_call(refusal, "weivel_standard_read_manifest was given no JSON text",
                                           "pass the manifest's text");
    }
    root = weivel_json_parse(json, length, &error);
    if (root == NULL) {
        return error.out_of_memory ? WEIVEL_STATUS_OUT_OF_MEMORY
                                   : weivel_standard_refuse_malformed_json(refusal, &error);
    }
    status = convert(root, manifest, refusal);
    weivel_json_free(root);
    return status;
}

const weivel_manifest *weivel_standard_manifest_view(const weivel_standard_manifest *manifest) {
    return manifest == NULL ? NULL : &manifest->view;
}

void weivel_standard_manifest_release(weivel_standard_manifest *manifest) {
    size_t at;
    if (manifest == NULL) {
        return;
    }
    for (at = 0; at < manifest->block_count; at++) {
        weivel_standard_memory_release(manifest->blocks[at]);
    }
    weivel_standard_memory_release(manifest->blocks);
    weivel_standard_memory_release(manifest);
}
