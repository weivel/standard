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
    .size = (uint32_t)sizeof(described_releases), .reflections = 0, .trees = 0, .refusals = 0, .cursors = 0};

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
    if (depth > WEIVEL_STANDARD_VALUE_DEPTH_LIMIT) {
        return WEIVEL_STATUS_MALFORMED;
    }
    *to = *from;
    to->text = NULL;
    to->items = NULL;
    to->entries = NULL;
    if (from->kind == WEIVEL_STANDARD_VALUE_TEXT) {
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
    memset(&to->held, 0, sizeof to->held);
    if (!copy_text(arena, from->label, &to->label) || !copy_point(arena, &from->origin, &to->origin)) {
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    if ((status = copy_tags(arena, from, to)) != WEIVEL_STATUS_OK ||
        (status = copy_scalar_arrays(arena, from, to)) != WEIVEL_STATUS_OK ||
        (status = copy_best(arena, from, to)) != WEIVEL_STATUS_OK ||
        (status = copy_points(arena, from, to)) != WEIVEL_STATUS_OK ||
        (status = copy_value(arena, &from->held, &to->held, 0)) != WEIVEL_STATUS_OK) {
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
    if ((status = tags_node(arena, sample, &entries[TREE_TAGS].value)) != WEIVEL_STATUS_OK ||
        (status = copy_value(arena, &sample->held, &entries[TREE_HELD].value, 1)) != WEIVEL_STATUS_OK) {
        return status;
    }
    if (entries[TREE_HELD].value.kind == WEIVEL_STANDARD_VALUE_NONE) {
        entries[TREE_HELD].value.kind = WEIVEL_STANDARD_VALUE_NULL;
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

static weivel_status mirror_count_releases(void *context, described_releases *releases) {
    uint32_t size;
    (void)context;
    if (releases == NULL || releases->size < sizeof(described_releases)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    size = releases->size;
    *releases = released;
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

static void release_refusal(void *context, weivel_standard_service_refusal *refusal) {
    (void)context;
    if (refusal == NULL || refusal->size < sizeof(weivel_standard_service_refusal) || refusal->storage == NULL) {
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

static const described_mirror mirror = {.size = (uint32_t)sizeof(described_mirror),
                                        .context = &library_context,
                                        .reflect = mirror_reflect,
                                        .tree = mirror_tree,
                                        .find = mirror_find,
                                        .measure = mirror_measure,
                                        .count_releases = mirror_count_releases,
                                        .release_reflection = mirror_release_reflection,
                                        .release_tree = mirror_release_tree,
                                        .release_refusal = release_refusal};

static const described_walker walker = {.size = (uint32_t)sizeof(described_walker),
                                        .context = &library_context,
                                        .open = walker_open,
                                        .use = walker_use,
                                        .visit = walker_visit,
                                        .release_cursor = walker_release_cursor,
                                        .release_refusal = release_refusal};

const void *described_mirror_definition(void) {
    return &mirror;
}

const void *described_walker_definition(void) {
    return &walker;
}
