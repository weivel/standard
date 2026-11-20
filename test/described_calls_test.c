#include <stdio.h>
#include <string.h>

#include "check.h"
#include "described/described.h"
#include "described/loader.h"

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

/** Above 2^53, so a binding that carries a u64 through a double loses it. */
static const uint64_t BEYOND_DOUBLE = ((uint64_t)1 << 53) + 1;

typedef struct sample_parts {
    const char *tags[3];
    uint8_t bytes[3];
    described_point best;
    described_point points[2];
    uint32_t numbers[2];
    weivel_standard_value held_items[3];
    weivel_standard_entry extra_entries[1];
    weivel_standard_value extra;
} sample_parts;

static described_sample rich_sample(sample_parts *parts) {
    described_sample sample = WEIVEL_INIT(described_sample);
    memset(parts, 0, sizeof *parts);
    parts->tags[0] = "one";
    parts->tags[1] = NULL;
    parts->tags[2] = "three";
    parts->bytes[0] = 0;
    parts->bytes[1] = 255;
    parts->bytes[2] = 7;
    parts->best.label = "best";
    parts->best.x = 2;
    parts->best.y = 2.5;
    parts->points[0].label = "first point";
    parts->points[0].x = INT64_MIN;
    parts->points[0].y = -0.5;
    parts->points[1].label = NULL;
    parts->points[1].x = INT64_MAX;
    parts->points[1].y = 1e300;
    parts->numbers[0] = 7;
    parts->numbers[1] = UINT32_MAX;
    parts->held_items[0].kind = WEIVEL_STANDARD_VALUE_NUMBER;
    parts->held_items[0].number = 1.5;
    parts->held_items[1].kind = WEIVEL_STANDARD_VALUE_TEXT;
    parts->held_items[1].text = "two";
    parts->held_items[1].length = 3;
    parts->held_items[2].kind = WEIVEL_STANDARD_VALUE_BOOLEAN;
    parts->held_items[2].boolean = 1;
    parts->extra_entries[0].key = "nothing";
    parts->extra_entries[0].key_length = 7;
    parts->extra_entries[0].value.kind = WEIVEL_STANDARD_VALUE_NULL;
    parts->extra.kind = WEIVEL_STANDARD_VALUE_RECORD;
    parts->extra.entries = parts->extra_entries;
    parts->extra.count = 1;
    sample.shade = DESCRIBED_SHADE_DARK;
    sample.tier = DESCRIBED_TIER_HIGH;
    sample.flags = 5;
    sample.big = BEYOND_DOUBLE;
    sample.delta = -42;
    sample.ratio = 0.25;
    sample.label = "reflected";
    sample.tags = parts->tags;
    sample.tag_count = COUNT_OF(parts->tags);
    sample.bytes = parts->bytes;
    sample.length = COUNT_OF(parts->bytes);
    sample.origin.label = "origin";
    sample.origin.x = -1;
    sample.origin.y = 1.25;
    sample.best = &parts->best;
    sample.points = parts->points;
    sample.point_count = COUNT_OF(parts->points);
    sample.numbers = parts->numbers;
    sample.number_count = COUNT_OF(parts->numbers);
    sample.held.kind = WEIVEL_STANDARD_VALUE_LIST;
    sample.held.items = parts->held_items;
    sample.held.count = COUNT_OF(parts->held_items);
    sample.extra = &parts->extra;
    return sample;
}

static int same_text(const char *actual, const char *expected) {
    return actual == NULL ? expected == NULL : expected != NULL && strcmp(actual, expected) == 0;
}

/** Equal and, unless both are NULL, not the same memory, since an answer owns its copy. */
static int copied_text(const char *actual, const char *expected) {
    return same_text(actual, expected) && (actual == NULL || actual != expected);
}

static int same_point(const described_point *actual, const described_point *expected) {
    return copied_text(actual->label, expected->label) && actual->x == expected->x && actual->y == expected->y;
}

static int same_value(const weivel_standard_value *actual, const weivel_standard_value *expected);

static int same_children(const weivel_standard_value *actual, const weivel_standard_value *expected) {
    size_t at;
    for (at = 0; at < expected->count; at++) {
        if (expected->kind == WEIVEL_STANDARD_VALUE_LIST && !same_value(&actual->items[at], &expected->items[at])) {
            return 0;
        }
        if (expected->kind == WEIVEL_STANDARD_VALUE_RECORD &&
            (actual->entries[at].key_length != expected->entries[at].key_length ||
             !copied_text(actual->entries[at].key, expected->entries[at].key) ||
             !same_value(&actual->entries[at].value, &expected->entries[at].value))) {
            return 0;
        }
    }
    return 1;
}

static int same_value(const weivel_standard_value *actual, const weivel_standard_value *expected) {
    if (actual->kind != expected->kind || actual->boolean != expected->boolean || actual->number != expected->number ||
        actual->length != expected->length || actual->count != expected->count ||
        !copied_text(actual->text, expected->text)) {
        return 0;
    }
    return same_children(actual, expected);
}

static void check_scalars(const described_sample *actual, const described_sample *expected) {
    CHECK(actual->size == sizeof(described_sample));
    CHECK(actual->shade == DESCRIBED_SHADE_DARK && actual->tier == DESCRIBED_TIER_HIGH && actual->flags == 5);
    CHECK(actual->big == BEYOND_DOUBLE);
    CHECK(actual->delta == -42 && actual->ratio == 0.25);
    CHECK(copied_text(actual->label, expected->label));
    CHECK(same_point(&actual->origin, &expected->origin));
}

static void check_arrays(const described_sample *actual, const described_sample *expected) {
    size_t at;
    CHECK(actual->tag_count == expected->tag_count && actual->tags != expected->tags);
    for (at = 0; at < actual->tag_count && at < expected->tag_count; at++) {
        CHECK(copied_text(actual->tags[at], expected->tags[at]));
    }
    CHECK(actual->length == expected->length && actual->bytes != expected->bytes);
    CHECK(actual->length == expected->length && memcmp(actual->bytes, expected->bytes, expected->length) == 0);
    CHECK(actual->best != NULL && actual->best != expected->best && same_point(actual->best, expected->best));
    CHECK(actual->point_count == expected->point_count && actual->points != expected->points);
    for (at = 0; at < actual->point_count && at < expected->point_count; at++) {
        CHECK(same_point(&actual->points[at], &expected->points[at]));
    }
    CHECK(actual->number_count == expected->number_count && actual->numbers != expected->numbers);
    CHECK(actual->number_count == expected->number_count &&
          memcmp(actual->numbers, expected->numbers, expected->number_count * sizeof *expected->numbers) == 0);
}

static void check_values(const described_sample *actual, const described_sample *expected) {
    CHECK(actual->held.items != expected->held.items && same_value(&actual->held, &expected->held));
    CHECK(actual->extra != NULL && actual->extra != expected->extra && same_value(actual->extra, expected->extra));
}

static described_releases releases_of(const described_mirror *mirror) {
    described_releases releases = WEIVEL_INIT(described_releases);
    CHECK(mirror->count_releases(mirror->context, &releases) == WEIVEL_STATUS_OK);
    CHECK(releases.size == sizeof(described_releases));
    return releases;
}

static void reflects_every_kind_value_for_value(const described_mirror *mirror) {
    sample_parts parts;
    described_sample sample = rich_sample(&parts);
    described_reflection reflection = WEIVEL_INIT(described_reflection);
    described_releases before = releases_of(mirror);
    CHECK(mirror->reflect(mirror->context, &sample, &reflection, NULL) == WEIVEL_STATUS_OK);
    CHECK(reflection.size == sizeof(described_reflection) && reflection.storage != NULL);
    check_scalars(&reflection.sample, &sample);
    check_arrays(&reflection.sample, &sample);
    check_values(&reflection.sample, &sample);
    mirror->release_reflection(mirror->context, &reflection);
    CHECK(reflection.size == sizeof(described_reflection) && reflection.storage == NULL);
    CHECK(reflection.sample.label == NULL && reflection.sample.tags == NULL);
    CHECK(releases_of(mirror).reflections == before.reflections + 1);
    mirror->release_reflection(mirror->context, &reflection);
    CHECK(releases_of(mirror).reflections == before.reflections + 1);
}

static void reflects_an_empty_sample_into_an_answer_that_still_owns_storage(const described_mirror *mirror) {
    described_sample sample = WEIVEL_INIT(described_sample);
    described_reflection reflection = WEIVEL_INIT(described_reflection);
    described_releases before = releases_of(mirror);
    CHECK(mirror->reflect(mirror->context, &sample, &reflection, NULL) == WEIVEL_STATUS_OK);
    CHECK(reflection.storage != NULL && reflection.sample.label == NULL && reflection.sample.tags == NULL);
    CHECK(reflection.sample.extra == NULL && reflection.sample.held.kind == WEIVEL_STANDARD_VALUE_NONE);
    mirror->release_reflection(mirror->context, &reflection);
    CHECK(releases_of(mirror).reflections == before.reflections + 1);
}

static void answers_malformed_for_what_it_cannot_read(const described_mirror *mirror) {
    sample_parts parts;
    described_sample sample = rich_sample(&parts);
    described_sample short_sample = WEIVEL_INIT(described_sample);
    described_reflection reflection = WEIVEL_INIT(described_reflection);
    described_reflection holding = WEIVEL_INIT(described_reflection);
    int storage;
    holding.storage = &storage;
    short_sample.size = (uint32_t)offsetof(described_sample, extra);
    CHECK(mirror->reflect(mirror->context, NULL, &reflection, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(mirror->reflect(mirror->context, &sample, NULL, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(mirror->reflect(mirror->context, &short_sample, &reflection, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(mirror->reflect(mirror->context, &sample, &holding, NULL) == WEIVEL_STATUS_MALFORMED);
    sample.tags = NULL;
    CHECK(mirror->reflect(mirror->context, &sample, &reflection, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(reflection.storage == NULL && holding.storage == &storage);
}

static void check_refusal(weivel_standard_service_refusal *refusal) {
    check_text(refusal->detail, DESCRIBED_REFUSAL_DETAIL, "the refusal's detail");
    check_text(refusal->fix, DESCRIBED_REFUSAL_FIX, "the refusal's fix");
    CHECK(refusal->storage != NULL);
}

static void refuses_with_its_known_texts(const described_mirror *mirror) {
    sample_parts parts;
    described_sample sample = rich_sample(&parts);
    described_reflection reflection = WEIVEL_INIT(described_reflection);
    weivel_standard_service_refusal refusal = WEIVEL_INIT(weivel_standard_service_refusal);
    described_releases before = releases_of(mirror);
    sample.label = "refuse";
    CHECK(mirror->reflect(mirror->context, &sample, &reflection, &refusal) == WEIVEL_STATUS_REFUSED);
    CHECK(reflection.storage == NULL);
    check_refusal(&refusal);
    mirror->release_refusal(mirror->context, &refusal);
    CHECK(refusal.size == sizeof refusal && refusal.detail == NULL && refusal.fix == NULL && refusal.storage == NULL);
    CHECK(mirror->reflect(mirror->context, &sample, &reflection, NULL) == WEIVEL_STATUS_REFUSED);
    CHECK(releases_of(mirror).refusals == before.refusals + 1);
}

static const weivel_standard_entry *entry_at(const weivel_standard_value *record, size_t at, const char *key) {
    CHECK(at < record->count);
    if (at >= record->count) {
        return NULL;
    }
    check_text(record->entries[at].key, key, "a tree's key");
    CHECK(record->entries[at].key_length == strlen(key));
    return &record->entries[at];
}

static void check_tree(const weivel_standard_value *root, const described_sample *sample) {
    const weivel_standard_entry *label;
    const weivel_standard_entry *tags;
    const weivel_standard_entry *ratio;
    const weivel_standard_entry *held;
    CHECK(root->kind == WEIVEL_STANDARD_VALUE_RECORD && root->count == 4);
    if (root->kind != WEIVEL_STANDARD_VALUE_RECORD || root->count != 4) {
        return;
    }
    label = entry_at(root, 0, "label");
    tags = entry_at(root, 1, "tags");
    ratio = entry_at(root, 2, "ratio");
    held = entry_at(root, 3, "held");
    CHECK(label->value.kind == WEIVEL_STANDARD_VALUE_TEXT && label->value.length == strlen(sample->label));
    check_text(label->value.text, sample->label, "the tree's label");
    CHECK(tags->value.kind == WEIVEL_STANDARD_VALUE_LIST && tags->value.count == 3);
    if (tags->value.count == 3) {
        check_text(tags->value.items[0].text, "one", "the first tag");
        CHECK(tags->value.items[1].kind == WEIVEL_STANDARD_VALUE_NULL);
        check_text(tags->value.items[2].text, "three", "the third tag");
    }
    CHECK(ratio->value.kind == WEIVEL_STANDARD_VALUE_NUMBER && ratio->value.number == 0.25);
    CHECK(same_value(&held->value, &sample->held));
}

static void answers_a_value_tree_built_from_its_input(const described_mirror *mirror) {
    sample_parts parts;
    described_sample sample = rich_sample(&parts);
    weivel_standard_value_tree tree = WEIVEL_INIT(weivel_standard_value_tree);
    described_releases before = releases_of(mirror);
    CHECK(mirror->tree(mirror->context, &sample, &tree, NULL) == WEIVEL_STATUS_OK);
    CHECK(tree.storage != NULL);
    check_tree(&tree.root, &sample);
    mirror->release_tree(mirror->context, &tree);
    CHECK(tree.size == sizeof tree && tree.storage == NULL && tree.root.kind == WEIVEL_STANDARD_VALUE_NONE);
    CHECK(releases_of(mirror).trees == before.trees + 1);
    CHECK(mirror->tree(mirror->context, NULL, &tree, NULL) == WEIVEL_STATUS_MALFORMED);
}

static void finds_none_and_refuses_by_key(const described_mirror *mirror) {
    const char *found = NULL;
    weivel_standard_service_refusal refusal = WEIVEL_INIT(weivel_standard_service_refusal);
    CHECK(mirror->find(mirror->context, "alpha", &found, NULL) == WEIVEL_STATUS_OK);
    check_text(found, "the first letter", "alpha");
    CHECK(mirror->find(mirror->context, "omega", &found, NULL) == WEIVEL_STATUS_OK);
    check_text(found, "the last letter", "omega");
    found = NULL;
    CHECK(mirror->find(mirror->context, "beta", &found, NULL) == WEIVEL_STATUS_NONE);
    CHECK(found == NULL);
    CHECK(mirror->find(mirror->context, "refuse", &found, &refusal) == WEIVEL_STATUS_REFUSED);
    CHECK(found == NULL);
    check_refusal(&refusal);
    mirror->release_refusal(mirror->context, &refusal);
    CHECK(mirror->find(mirror->context, NULL, &found, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(mirror->find(mirror->context, "alpha", NULL, NULL) == WEIVEL_STATUS_MALFORMED);
}

static void measures_bytes_by_pointer_and_length(const described_mirror *mirror) {
    static const uint8_t bytes[] = {1, 2, 250, 0};
    uint64_t sum = 99;
    CHECK(mirror->measure(mirror->context, bytes, sizeof bytes, &sum) == WEIVEL_STATUS_OK && sum == 253);
    CHECK(mirror->measure(mirror->context, NULL, 0, &sum) == WEIVEL_STATUS_OK && sum == 0);
    CHECK(mirror->measure(mirror->context, NULL, 3, &sum) == WEIVEL_STATUS_MALFORMED);
    CHECK(mirror->measure(mirror->context, bytes, sizeof bytes, NULL) == WEIVEL_STATUS_MALFORMED);
}

static void walks_through_a_handle(const described_mirror *mirror, const described_walker *walker) {
    static const uint64_t steps[] = {3, 5};
    described_cursor *cursor = NULL;
    described_visit visit = WEIVEL_INIT(described_visit);
    uint64_t start = 0;
    uint64_t position = 0;
    described_releases before = releases_of(mirror);
    CHECK(walker->open(walker->context, "walk", BEYOND_DOUBLE, &cursor, NULL) == WEIVEL_STATUS_OK);
    CHECK(cursor != NULL);
    if (cursor == NULL) {
        return;
    }
    CHECK(walker->use(walker->context, cursor, &start) == WEIVEL_STATUS_OK && start == BEYOND_DOUBLE);
    visit.cursor = cursor;
    visit.skip = 2;
    visit.steps = steps;
    visit.step_count = COUNT_OF(steps);
    CHECK(walker->visit(walker->context, &visit, &position) == WEIVEL_STATUS_OK && position == BEYOND_DOUBLE + 10);
    visit.cursor = NULL;
    CHECK(walker->visit(walker->context, &visit, &position) == WEIVEL_STATUS_MALFORMED);
    CHECK(walker->use(walker->context, NULL, &start) == WEIVEL_STATUS_MALFORMED);
    CHECK(walker->open(walker->context, "again", 1, &cursor, NULL) == WEIVEL_STATUS_MALFORMED);
    walker->release_cursor(walker->context, cursor);
    walker->release_cursor(walker->context, NULL);
    CHECK(releases_of(mirror).cursors == before.cursors + 1);
}

static void refuses_to_open_by_label(const described_mirror *mirror, const described_walker *walker) {
    described_cursor *cursor = NULL;
    weivel_standard_service_refusal refusal = WEIVEL_INIT(weivel_standard_service_refusal);
    described_releases before = releases_of(mirror);
    CHECK(walker->open(walker->context, "refuse", 1, &cursor, &refusal) == WEIVEL_STATUS_REFUSED);
    CHECK(cursor == NULL);
    check_refusal(&refusal);
    walker->release_refusal(walker->context, &refusal);
    CHECK(refusal.storage == NULL);
    CHECK(walker->open(walker->context, NULL, 1, &cursor, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(releases_of(mirror).refusals == before.refusals + 1);
    CHECK(releases_of(mirror).cursors == before.cursors);
}

static void counts_nothing_for_an_empty_release(const described_mirror *mirror, const described_walker *walker) {
    described_reflection reflection = WEIVEL_INIT(described_reflection);
    weivel_standard_value_tree tree = WEIVEL_INIT(weivel_standard_value_tree);
    weivel_standard_service_refusal refusal = WEIVEL_INIT(weivel_standard_service_refusal);
    described_releases before = releases_of(mirror);
    described_releases after;
    described_releases short_releases = WEIVEL_INIT(described_releases);
    mirror->release_reflection(mirror->context, &reflection);
    mirror->release_reflection(mirror->context, NULL);
    mirror->release_tree(mirror->context, &tree);
    mirror->release_refusal(mirror->context, &refusal);
    walker->release_refusal(walker->context, NULL);
    after = releases_of(mirror);
    CHECK(memcmp(&before, &after, sizeof before) == 0);
    short_releases.size = (uint32_t)offsetof(described_releases, cursors);
    CHECK(mirror->count_releases(mirror->context, &short_releases) == WEIVEL_STATUS_MALFORMED);
}

static void calls_every_operation(described_library library) {
    described_definition_symbol mirror_symbol = described_library_definition(library, "described_mirror_definition");
    described_definition_symbol walker_symbol = described_library_definition(library, "described_walker_definition");
    const described_mirror *mirror;
    const described_walker *walker;
    CHECK(mirror_symbol != NULL && walker_symbol != NULL);
    if (mirror_symbol == NULL || walker_symbol == NULL) {
        return;
    }
    mirror = mirror_symbol();
    walker = walker_symbol();
    CHECK(mirror->size == sizeof(described_mirror) && walker->size == sizeof(described_walker));
    CHECK(mirror->context != NULL && mirror->context == walker->context);
    reflects_every_kind_value_for_value(mirror);
    reflects_an_empty_sample_into_an_answer_that_still_owns_storage(mirror);
    answers_malformed_for_what_it_cannot_read(mirror);
    refuses_with_its_known_texts(mirror);
    answers_a_value_tree_built_from_its_input(mirror);
    finds_none_and_refuses_by_key(mirror);
    measures_bytes_by_pointer_and_length(mirror);
    walks_through_a_handle(mirror, walker);
    refuses_to_open_by_label(mirror, walker);
    counts_nothing_for_an_empty_release(mirror, walker);
}

int main(int argument_count, char **arguments) {
    described_library library;
    CHECK(argument_count == 2);
    if (argument_count != 2 || !described_library_open(arguments[1], &library)) {
        fprintf(stderr, "the described library cannot be loaded\n");
        return 1;
    }
    calls_every_operation(library);
    CHECK(described_library_close(library));
    return check_summary("described_calls_test");
}
