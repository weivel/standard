#include "described.h"

#include <stdlib.h>
#include <string.h>

/** One allocation of an arena, its bytes following the header at an alignment every member type accepts. */
typedef union described_block {
    union described_block *next;
    long double widest_float;
    uint64_t widest_integer;
    void (*function)(void);
} described_block;

/** Everything one answer owns, freed together by the release for its kind. */
typedef struct described_arena {
    described_block *blocks;
} described_arena;

struct described_cursor {
    uint64_t start;
};

static int library_context;

static described_releases released = {
    .size = (uint32_t)sizeof(described_releases), .reflections = 0, .trees = 0, .refusals = 0, .cursors = 0,
    .nested_refusals = 0, .outlines = 0};

static described_arena *arena_open(void) {
    described_arena *arena = malloc(sizeof *arena);
    if (arena != NULL) {
        arena->blocks = NULL;
    }
    return arena;
}

static void arena_close(described_arena *arena) {
    while (arena->blocks != NULL) {
        described_block *next = arena->blocks->next;
        free(arena->blocks);
        arena->blocks = next;
    }
    free(arena);
}

static void *arena_take(described_arena *arena, size_t count, size_t size) {
    described_block *block;
    if (count != 0 && size > ((size_t)-1 - sizeof(described_block)) / count) {
        return NULL;
    }
    block = malloc(sizeof(described_block) + count * size);
    if (block == NULL) {
        return NULL;
    }
    block->next = arena->blocks;
    arena->blocks = block;
    return block + 1;
}

/** A copy of `length` bytes of `text` and a NUL, or NULL when `text` is. 0 when memory runs out. */
static int copy_bytes_of(described_arena *arena, const char *text, size_t length, const char **copy) {
    char *made;
    *copy = NULL;
    if (text == NULL) {
        return 1;
    }
    made = arena_take(arena, length + 1, 1);
    if (made == NULL) {
        return 0;
    }
    memcpy(made, text, length);
    made[length] = '\0';
    *copy = made;
    return 1;
}

static int copy_text(described_arena *arena, const char *text, const char **copy) {
    return copy_bytes_of(arena, text, text == NULL ? 0 : strlen(text), copy);
}

/** Whether a text of `length` bytes can be copied: NULL only when empty, and short enough for its NUL. */
static int is_copyable_text(const char *text, size_t length) {
    return length != SIZE_MAX && (text != NULL || length == 0);
}

static int is_refused(const char *text) {
    return text != NULL && strcmp(text, "refuse") == 0;
}

static int refusal_is_usable(const weivel_standard_service_refusal *refusal) {
    return refusal == NULL || (refusal->size >= sizeof(weivel_standard_service_refusal) && refusal->storage == NULL);
}

static weivel_status refuse(weivel_standard_service_refusal *refusal) {
    described_arena *arena;
    if (refusal == NULL) {
        return WEIVEL_STATUS_REFUSED;
    }
    arena = arena_open();
    if (arena == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    if (!copy_text(arena, DESCRIBED_REFUSAL_DETAIL, &refusal->detail) ||
        !copy_text(arena, DESCRIBED_REFUSAL_FIX, &refusal->fix)) {
        arena_close(arena);
        refusal->detail = NULL;
        refusal->fix = NULL;
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    refusal->storage = arena;
    return WEIVEL_STATUS_REFUSED;
}

static weivel_status copy_value(described_arena *arena, const weivel_standard_value *from, weivel_standard_value *to,
                                size_t depth);

static weivel_status copy_items(described_arena *arena, const weivel_standard_value *from, weivel_standard_value *to,
                                size_t depth) {
    weivel_standard_value *items;
    size_t at;
    if (from->count == 0) {
        return WEIVEL_STATUS_OK;
    }
    if (from->items == NULL) {
        return WEIVEL_STATUS_MALFORMED;
    }
    items = arena_take(arena, from->count, sizeof *items);
    if (items == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    for (at = 0; at < from->count; at++) {
        weivel_status status = copy_value(arena, &from->items[at], &items[at], depth + 1);
        if (status != WEIVEL_STATUS_OK) {
            return status;
        }
    }
    to->items = items;
    return WEIVEL_STATUS_OK;
}

static weivel_status copy_entries(described_arena *arena, const weivel_standard_value *from,
                                  weivel_standard_value *to, size_t depth) {
    weivel_standard_entry *entries;
    size_t at;
    if (from->count == 0) {
        return WEIVEL_STATUS_OK;
    }
    if (from->entries == NULL) {
        return WEIVEL_STATUS_MALFORMED;
    }
    entries = arena_take(arena, from->count, sizeof *entries);
    if (entries == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    for (at = 0; at < from->count; at++) {
        weivel_status status;
        if (!is_copyable_text(from->entries[at].key, from->entries[at].key_length)) {
            return WEIVEL_STATUS_MALFORMED;
        }
        entries[at].key_length = from->entries[at].key_length;
        if (!copy_bytes_of(arena, from->entries[at].key, from->entries[at].key_length, &entries[at].key)) {
            return WEIVEL_STATUS_OUT_OF_MEMORY;
        }
        status = copy_value(arena, &from->entries[at].value, &entries[at].value, depth + 1);
        if (status != WEIVEL_STATUS_OK) {
            return status;
        }
    }
    to->entries = entries;
    return WEIVEL_STATUS_OK;
}

static weivel_status copy_value(described_arena *arena, const weivel_standard_value *from, weivel_standard_value *to,
                                size_t depth) {
    if (depth > WEIVEL_STANDARD_VALUE_DEPTH_LIMIT || (depth > 0 && from->kind == WEIVEL_STANDARD_VALUE_NONE)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    *to = *from;
    to->text = NULL;
    to->items = NULL;
    to->entries = NULL;
    if (from->kind == WEIVEL_STANDARD_VALUE_TEXT) {
        if (!is_copyable_text(from->text, from->length)) {
            return WEIVEL_STATUS_MALFORMED;
        }
        return copy_bytes_of(arena, from->text, from->length, &to->text) ? WEIVEL_STATUS_OK
                                                                          : WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    if (from->kind == WEIVEL_STANDARD_VALUE_LIST) {
        return copy_items(arena, from, to, depth);
    }
    if (from->kind == WEIVEL_STANDARD_VALUE_RECORD) {
        return copy_entries(arena, from, to, depth);
    }
    return WEIVEL_STATUS_OK;
}

static int copy_point(described_arena *arena, const described_point *from, described_point *to) {
    *to = *from;
    return copy_text(arena, from->label, &to->label);
}

/** A copy of `count` elements of `size` bytes, or NULL for none. MALFORMED for a count beside a NULL array. */
static weivel_status copy_array(described_arena *arena, const void *from, size_t count, size_t size, void **copy) {
    *copy = NULL;
    if (count == 0) {
        return WEIVEL_STATUS_OK;
    }
    if (from == NULL) {
        return WEIVEL_STATUS_MALFORMED;
    }
    *copy = arena_take(arena, count, size);
    if (*copy == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    memcpy(*copy, from, count * size);
    return WEIVEL_STATUS_OK;
}

static weivel_status copy_tags(described_arena *arena, const described_sample *from, described_sample *to) {
    void *array;
    const char **tags;
    size_t at;
    weivel_status status = copy_array(arena, from->tags, from->tag_count, sizeof *from->tags, &array);
    if (status != WEIVEL_STATUS_OK) {
        return status;
    }
    tags = array;
    for (at = 0; at < from->tag_count; at++) {
        if (!copy_text(arena, from->tags[at], &tags[at])) {
            return WEIVEL_STATUS_OUT_OF_MEMORY;
        }
    }
    to->tags = tags;
    return WEIVEL_STATUS_OK;
}

static weivel_status copy_points(described_arena *arena, const described_sample *from, described_sample *to) {
    void *array;
    described_point *points;
    size_t at;
    weivel_status status = copy_array(arena, from->points, from->point_count, sizeof *from->points, &array);
    if (status != WEIVEL_STATUS_OK) {
        return status;
    }
    points = array;
    for (at = 0; at < from->point_count; at++) {
        if (!copy_point(arena, &from->points[at], &points[at])) {
            return WEIVEL_STATUS_OUT_OF_MEMORY;
        }
    }
    to->points = points;
    return WEIVEL_STATUS_OK;
}

static weivel_status copy_best(described_arena *arena, const described_sample *from, described_sample *to) {
    described_point *best;
    if (from->best == NULL) {
        return WEIVEL_STATUS_OK;
    }
    best = arena_take(arena, 1, sizeof *best);
    if (best == NULL || !copy_point(arena, from->best, best)) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    to->best = best;
    return WEIVEL_STATUS_OK;
}

static weivel_status copy_extra(described_arena *arena, const described_sample *from, described_sample *to) {
    weivel_standard_value *extra;
    weivel_status status;
    if (from->extra == NULL) {
        return WEIVEL_STATUS_OK;
    }
    extra = arena_take(arena, 1, sizeof *extra);
    if (extra == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    status = copy_value(arena, from->extra, extra, 0);
    to->extra = extra;
    return status;
}

static weivel_status copy_values(described_arena *arena, const described_sample *from, described_sample *to) {
    weivel_standard_value *values;
    size_t at;
    if (from->value_count == 0) {
        return WEIVEL_STATUS_OK;
    }
    if (from->values == NULL) {
        return WEIVEL_STATUS_MALFORMED;
    }
    values = arena_take(arena, from->value_count, sizeof *values);
    if (values == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    for (at = 0; at < from->value_count; at++) {
        weivel_status status = copy_value(arena, &from->values[at], &values[at], 0);
        if (status != WEIVEL_STATUS_OK) {
            return status;
        }
    }
    to->values = values;
    return WEIVEL_STATUS_OK;
}

static weivel_status copy_scalar_arrays(described_arena *arena, const described_sample *from, described_sample *to) {
    void *bytes;
    void *numbers;
    weivel_status status = copy_array(arena, from->bytes, from->length, 1, &bytes);
    if (status != WEIVEL_STATUS_OK) {
        return status;
    }
    to->bytes = bytes;
    status = copy_array(arena, from->numbers, from->number_count, sizeof *from->numbers, &numbers);
    to->numbers = numbers;
    return status;
}

static weivel_status copy_sample(described_arena *arena, const described_sample *from, described_sample *to) {
    weivel_status status;
    *to = *from;
    to->size = (uint32_t)sizeof(described_sample);
    to->tags = NULL;
    to->bytes = NULL;
    to->best = NULL;
    to->points = NULL;
    to->numbers = NULL;
    to->extra = NULL;
    to->values = NULL;
    memset(&to->held, 0, sizeof to->held);
    if (!copy_text(arena, from->label, &to->label) || !copy_point(arena, &from->origin, &to->origin)) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    if ((status = copy_tags(arena, from, to)) != WEIVEL_STATUS_OK ||
        (status = copy_scalar_arrays(arena, from, to)) != WEIVEL_STATUS_OK ||
        (status = copy_best(arena, from, to)) != WEIVEL_STATUS_OK ||
        (status = copy_points(arena, from, to)) != WEIVEL_STATUS_OK ||
        (status = copy_value(arena, &from->held, &to->held, 0)) != WEIVEL_STATUS_OK ||
        (status = copy_values(arena, from, to)) != WEIVEL_STATUS_OK) {
        return status;
    }
    return copy_extra(arena, from, to);
}

static int sample_is_readable(const described_sample *sample) {
    return sample != NULL && sample->size >= sizeof(described_sample);
}

static weivel_status mirror_reflect(void *context, const described_sample *sample, described_reflection *reflection,
                                    weivel_standard_service_refusal *refusal) {
    described_arena *arena;
    described_sample copy;
    weivel_status status;
    (void)context;
    if (!sample_is_readable(sample) || reflection == NULL || reflection->size < sizeof(described_reflection) ||
        reflection->storage != NULL || !refusal_is_usable(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    if (is_refused(sample->label)) {
        return refuse(refusal);
    }
    arena = arena_open();
    if (arena == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    status = copy_sample(arena, sample, &copy);
    if (status != WEIVEL_STATUS_OK) {
        arena_close(arena);
        return status;
    }
    reflection->sample = copy;
    reflection->storage = arena;
    return WEIVEL_STATUS_OK;
}

static weivel_status copy_node(described_arena *arena, const described_node *from, described_node *to, size_t depth);

static weivel_status copy_next(described_arena *arena, const described_node *from, described_node *to,
                               size_t depth) {
    described_node *next;
    weivel_status status;
    if (from->next == NULL) {
        return WEIVEL_STATUS_OK;
    }
    next = arena_take(arena, 1, sizeof *next);
    if (next == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    status = copy_node(arena, from->next, next, depth + 1);
    to->next = next;
    return status;
}

static weivel_status copy_content(described_arena *arena, const described_node *from, described_node *to,
                                  size_t depth) {
    described_node *content;
    size_t at;
    if (from->content_count == 0) {
        return WEIVEL_STATUS_OK;
    }
    content = arena_take(arena, from->content_count, sizeof *content);
    if (content == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    for (at = 0; at < from->content_count; at++) {
        weivel_status status = copy_node(arena, &from->content[at], &content[at], depth + 1);
        if (status != WEIVEL_STATUS_OK) {
            return status;
        }
    }
    to->content = content;
    return WEIVEL_STATUS_OK;
}

/** Copies a node lying `depth` pointers below the root, and every node it reaches, setting each copy's depth. */
static weivel_status copy_node(described_arena *arena, const described_node *from, described_node *to, size_t depth) {
    weivel_status status;
    if (depth > DESCRIBED_OUTLINE_DEPTH_LIMIT || (from->content == NULL && from->content_count != 0)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    *to = *from;
    to->depth = (uint32_t)depth;
    to->next = NULL;
    to->content = NULL;
    if (!copy_text(arena, from->label, &to->label)) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    status = copy_next(arena, from, to, depth);
    return status == WEIVEL_STATUS_OK ? copy_content(arena, from, to, depth) : status;
}

static weivel_status mirror_outline(void *context, const described_node *node, described_outline *copy,
                                    weivel_standard_service_refusal *refusal) {
    described_arena *arena;
    described_node root;
    weivel_status status;
    (void)context;
    if (node == NULL || copy == NULL || copy->size < sizeof(described_outline) || copy->storage != NULL ||
        !refusal_is_usable(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    if (is_refused(node->label)) {
        return refuse(refusal);
    }
    arena = arena_open();
    if (arena == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    status = copy_node(arena, node, &root, 0);
    if (status != WEIVEL_STATUS_OK) {
        arena_close(arena);
        return status;
    }
    copy->root = root;
    copy->storage = arena;
    return WEIVEL_STATUS_OK;
}

static void mirror_release_outline(void *context, described_outline *copy) {
    (void)context;
    if (copy == NULL || copy->size < sizeof(described_outline) || copy->storage == NULL) {
        return;
    }
    arena_close(copy->storage);
    memset(&copy->root, 0, sizeof copy->root);
    copy->storage = NULL;
    released.outlines++;
}

static int text_node(described_arena *arena, const char *text, weivel_standard_value *node) {
    memset(node, 0, sizeof *node);
    if (text == NULL) {
        node->kind = WEIVEL_STANDARD_VALUE_NULL;
        return 1;
    }
    node->kind = WEIVEL_STANDARD_VALUE_TEXT;
    node->length = strlen(text);
    return copy_bytes_of(arena, text, node->length, &node->text);
}

static int key_of(described_arena *arena, const char *key, weivel_standard_entry *entry) {
    entry->key_length = strlen(key);
    return copy_bytes_of(arena, key, entry->key_length, &entry->key);
}

static weivel_status tags_node(described_arena *arena, const described_sample *sample, weivel_standard_value *node) {
    weivel_standard_value *items;
    size_t at;
    memset(node, 0, sizeof *node);
    node->kind = WEIVEL_STANDARD_VALUE_LIST;
    if (sample->tag_count == 0) {
        return WEIVEL_STATUS_OK;
    }
    if (sample->tags == NULL) {
        return WEIVEL_STATUS_MALFORMED;
    }
    items = arena_take(arena, sample->tag_count, sizeof *items);
    if (items == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    for (at = 0; at < sample->tag_count; at++) {
        if (!text_node(arena, sample->tags[at], &items[at])) {
            return WEIVEL_STATUS_OUT_OF_MEMORY;
        }
    }
    node->items = items;
    node->count = sample->tag_count;
    return WEIVEL_STATUS_OK;
}

enum { TREE_LABEL, TREE_TAGS, TREE_RATIO, TREE_HELD, TREE_ENTRIES };

static weivel_status build_record(described_arena *arena, const described_sample *sample,
                                  weivel_standard_value *root) {
    static const char *const keys[TREE_ENTRIES] = {"label", "tags", "ratio", "held"};
    weivel_standard_entry *entries = arena_take(arena, TREE_ENTRIES, sizeof *entries);
    weivel_status status;
    size_t at;
    if (entries == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    memset(entries, 0, TREE_ENTRIES * sizeof *entries);
    for (at = 0; at < TREE_ENTRIES; at++) {
        if (!key_of(arena, keys[at], &entries[at])) {
            return WEIVEL_STATUS_OUT_OF_MEMORY;
        }
    }
    if (!text_node(arena, sample->label, &entries[TREE_LABEL].value)) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    entries[TREE_RATIO].value.kind = WEIVEL_STANDARD_VALUE_NUMBER;
    entries[TREE_RATIO].value.number = sample->ratio;
    if ((status = tags_node(arena, sample, &entries[TREE_TAGS].value)) != WEIVEL_STATUS_OK) {
        return status;
    }
    if (sample->held.kind == WEIVEL_STANDARD_VALUE_NONE) {
        entries[TREE_HELD].value.kind = WEIVEL_STANDARD_VALUE_NULL;
    } else if ((status = copy_value(arena, &sample->held, &entries[TREE_HELD].value, 1)) != WEIVEL_STATUS_OK) {
        return status;
    }
    memset(root, 0, sizeof *root);
    root->kind = WEIVEL_STANDARD_VALUE_RECORD;
    root->entries = entries;
    root->count = TREE_ENTRIES;
    return WEIVEL_STATUS_OK;
}

static weivel_status mirror_tree(void *context, const described_sample *sample, weivel_standard_value_tree *tree,
                                 weivel_standard_service_refusal *refusal) {
    described_arena *arena;
    weivel_standard_value root;
    weivel_status status;
    (void)context;
    if (!sample_is_readable(sample) || tree == NULL || tree->size < sizeof(weivel_standard_value_tree) ||
        tree->storage != NULL || !refusal_is_usable(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    arena = arena_open();
    if (arena == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    status = build_record(arena, sample, &root);
    if (status != WEIVEL_STATUS_OK) {
        arena_close(arena);
        return status;
    }
    tree->root = root;
    tree->storage = arena;
    return WEIVEL_STATUS_OK;
}

static weivel_status mirror_find(void *context, const char *key, const char **found,
                                 weivel_standard_service_refusal *refusal) {
    (void)context;
    if (key == NULL || found == NULL || !refusal_is_usable(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    if (is_refused(key)) {
        return refuse(refusal);
    }
    if (strcmp(key, "alpha") == 0) {
        *found = "the first letter";
        return WEIVEL_STATUS_OK;
    }
    if (strcmp(key, "omega") == 0) {
        *found = "the last letter";
        return WEIVEL_STATUS_OK;
    }
    return WEIVEL_STATUS_NONE;
}

static weivel_status mirror_measure(void *context, const uint8_t *bytes, size_t length, uint64_t *sum) {
    uint64_t total = 0;
    size_t at;
    (void)context;
    if (sum == NULL || (bytes == NULL && length != 0)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    for (at = 0; at < length; at++) {
        total += bytes[at];
    }
    *sum = total;
    return WEIVEL_STATUS_OK;
}

static weivel_status mirror_negate(void *context, int64_t number, int64_t *negated) {
    (void)context;
    if (negated == NULL || number == INT64_MIN) {
        return WEIVEL_STATUS_MALFORMED;
    }
    *negated = -number;
    return WEIVEL_STATUS_OK;
}

static weivel_status mirror_halve(void *context, double number, double *half) {
    (void)context;
    if (half == NULL) {
        return WEIVEL_STATUS_MALFORMED;
    }
    *half = number / 2;
    return WEIVEL_STATUS_OK;
}

static weivel_status mirror_invert(void *context, uint32_t flags, uint32_t *inverted) {
    (void)context;
    if (inverted == NULL) {
        return WEIVEL_STATUS_MALFORMED;
    }
    *inverted = ~flags;
    return WEIVEL_STATUS_OK;
}

static weivel_status mirror_swap_shade(void *context, uint32_t shade, uint32_t *other) {
    (void)context;
    if (other == NULL || (shade != DESCRIBED_SHADE_LIGHT && shade != DESCRIBED_SHADE_DARK)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    *other = shade == DESCRIBED_SHADE_LIGHT ? DESCRIBED_SHADE_DARK : DESCRIBED_SHADE_LIGHT;
    return WEIVEL_STATUS_OK;
}

static weivel_status mirror_grow(void *context, size_t amount, size_t *grown) {
    (void)context;
    if (grown == NULL || amount == SIZE_MAX) {
        return WEIVEL_STATUS_MALFORMED;
    }
    *grown = amount + 1;
    return WEIVEL_STATUS_OK;
}

static weivel_status count_nodes(const weivel_standard_value *value, size_t depth, uint64_t *nodes) {
    size_t at;
    if (depth > WEIVEL_STANDARD_VALUE_DEPTH_LIMIT) {
        return WEIVEL_STATUS_MALFORMED;
    }
    (*nodes)++;
    if (value->kind != WEIVEL_STANDARD_VALUE_LIST && value->kind != WEIVEL_STANDARD_VALUE_RECORD) {
        return WEIVEL_STATUS_OK;
    }
    if (value->count != 0 && (value->kind == WEIVEL_STANDARD_VALUE_LIST ? value->items == NULL
                                                                        : value->entries == NULL)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    for (at = 0; at < value->count; at++) {
        const weivel_standard_value *child =
            value->kind == WEIVEL_STANDARD_VALUE_LIST ? &value->items[at] : &value->entries[at].value;
        weivel_status status = count_nodes(child, depth + 1, nodes);
        if (status != WEIVEL_STATUS_OK) {
            return status;
        }
    }
    return WEIVEL_STATUS_OK;
}

static weivel_status mirror_weigh(void *context, const weivel_standard_value *value, uint64_t *nodes) {
    uint64_t counted = 0;
    weivel_status status;
    (void)context;
    if (value == NULL || nodes == NULL) {
        return WEIVEL_STATUS_MALFORMED;
    }
    status = count_nodes(value, 0, &counted);
    if (status == WEIVEL_STATUS_OK) {
        *nodes = counted;
    }
    return status;
}

/** Fills as much of `releases` as its `size` covers, from its first shape up, leaving what lies beyond untouched. */
static weivel_status mirror_count_releases(void *context, described_releases *releases) {
    const size_t first_shape = offsetof(described_releases, nested_refusals) + sizeof released.nested_refusals;
    uint32_t size;
    (void)context;
    if (releases == NULL || releases->size < first_shape) {
        return WEIVEL_STATUS_MALFORMED;
    }
    size = releases->size;
    memcpy(releases, &released, size < sizeof released ? size : sizeof released);
    releases->size = size;
    return WEIVEL_STATUS_OK;
}

static void mirror_release_reflection(void *context, described_reflection *reflection) {
    (void)context;
    if (reflection == NULL || reflection->size < sizeof(described_reflection) || reflection->storage == NULL) {
        return;
    }
    arena_close(reflection->storage);
    memset(&reflection->sample, 0, sizeof reflection->sample);
    reflection->storage = NULL;
    released.reflections++;
}

static void mirror_release_tree(void *context, weivel_standard_value_tree *tree) {
    (void)context;
    if (tree == NULL || tree->size < sizeof(weivel_standard_value_tree) || tree->storage == NULL) {
        return;
    }
    arena_close(tree->storage);
    memset(&tree->root, 0, sizeof tree->root);
    tree->storage = NULL;
    released.trees++;
}

static uint64_t counter_start = DESCRIBED_COUNTER_START;

static void release_refusal(void *context, weivel_standard_service_refusal *refusal) {
    if (context == &counter_start || refusal == NULL || refusal->size < sizeof(weivel_standard_service_refusal) || refusal->storage == NULL) {
        return;
    }
    arena_close(refusal->storage);
    refusal->detail = NULL;
    refusal->fix = NULL;
    refusal->storage = NULL;
    released.refusals++;
}

static weivel_status walker_open(void *context, const char *label, uint64_t start, described_cursor **cursor,
                                 weivel_standard_service_refusal *refusal) {
    described_cursor *opened;
    (void)context;
    if (label == NULL || cursor == NULL || *cursor != NULL || !refusal_is_usable(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    if (is_refused(label)) {
        return refuse(refusal);
    }
    opened = malloc(sizeof *opened);
    if (opened == NULL) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    opened->start = start;
    *cursor = opened;
    return WEIVEL_STATUS_OK;
}

static weivel_status walker_use(void *context, const described_cursor *cursor, uint64_t *start) {
    (void)context;
    if (cursor == NULL || start == NULL) {
        return WEIVEL_STATUS_MALFORMED;
    }
    *start = cursor->start;
    return WEIVEL_STATUS_OK;
}

static weivel_status walker_visit(void *context, const described_visit *visit, uint64_t *position) {
    uint64_t landed;
    size_t at;
    (void)context;
    if (visit == NULL || visit->size < sizeof(described_visit) || visit->cursor == NULL || position == NULL ||
        (visit->steps == NULL && visit->step_count != 0)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    landed = visit->cursor->start + visit->skip;
    for (at = 0; at < visit->step_count; at++) {
        landed += visit->steps[at];
    }
    *position = landed;
    return WEIVEL_STATUS_OK;
}

static void walker_release_cursor(void *context, described_cursor *cursor) {
    (void)context;
    if (cursor == NULL) {
        return;
    }
    free(cursor);
    released.cursors++;
}

static weivel_status walker_wait(void *context, uint64_t ticks, uint64_t *waited) {
    (void)context;
    if (waited == NULL) {
        return WEIVEL_STATUS_MALFORMED;
    }
    *waited = ticks;
    return WEIVEL_STATUS_OK;
}

static weivel_status counter_add(void *context, uint64_t amount, uint64_t *total,
                                 weivel_standard_service_refusal *refusal) {
    uint64_t start = *(const uint64_t *)context;
    if (total == NULL || amount > UINT64_MAX - start || !refusal_is_usable(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    if (amount == DESCRIBED_COUNTER_REFUSED_AMOUNT) {
        return refuse(refusal);
    }
    *total = start + amount;
    return WEIVEL_STATUS_OK;
}

static void counter_release_refusal(void *context, weivel_standard_service_refusal *refusal) {
    if (context != &counter_start || refusal == NULL || refusal->size < sizeof(weivel_standard_service_refusal) ||
        refusal->storage == NULL) {
        return;
    }
    arena_close(refusal->storage);
    refusal->detail = NULL;
    refusal->fix = NULL;
    refusal->storage = NULL;
    released.nested_refusals++;
}

static const described_counter counter = {
    .size = (uint32_t)sizeof(described_counter), .context = &counter_start,
    .add = counter_add, .release_refusal = counter_release_refusal};

static weivel_status walker_lend(void *context, described_loan *loan) {
    (void)context;
    if (loan == NULL || loan->size < sizeof(described_loan)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    loan->counter = &counter;
    return WEIVEL_STATUS_OK;
}

#define MIRROR_BUT_HALVE                              \
    .size = (uint32_t)sizeof(described_mirror),       \
    .context = &library_context,                      \
    .reflect = mirror_reflect,                        \
    .tree = mirror_tree,                              \
    .find = mirror_find,                              \
    .measure = mirror_measure,                        \
    .negate = mirror_negate,                          \
    .invert = mirror_invert,                          \
    .swap_shade = mirror_swap_shade,                  \
    .grow = mirror_grow,                              \
    .weigh = mirror_weigh,                            \
    .count_releases = mirror_count_releases,          \
    .release_reflection = mirror_release_reflection,  \
    .release_tree = mirror_release_tree,              \
    .release_refusal = release_refusal,               \
    .outline = mirror_outline,                        \
    .release_outline = mirror_release_outline

static const described_mirror mirror = {MIRROR_BUT_HALVE, .halve = mirror_halve};

static const described_mirror mirror_without_halve = {MIRROR_BUT_HALVE};

static const described_walker walker = {.size = (uint32_t)sizeof(described_walker),
                                        .context = &library_context,
                                        .open = walker_open,
                                        .use = walker_use,
                                        .visit = walker_visit,
                                        .release_cursor = walker_release_cursor,
                                        .release_refusal = release_refusal,
                                        .wait = walker_wait,
                                        .lend = walker_lend};

const void *described_mirror_definition(void) {
    return &mirror;
}

const void *described_walker_definition(void) {
    return &walker;
}

const void *described_mirror_without_halve_definition(void) {
    return &mirror_without_halve;
}
