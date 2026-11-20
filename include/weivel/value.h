#ifndef WEIVEL_STANDARD_VALUE_H
#define WEIVEL_STANDARD_VALUE_H

/**
 * @file
 * One typed value tree, for data a definition only stores or hands back and whose shape belongs to someone else, as a
 * setting's value or a plugin's props. A field whose shape the definition decides is typed by its own struct, never
 * by this. It is the tree a description's `value` kind names (`weivel/description.h`), so every binding knows one
 * shape for it. No value crossing a definition is JSON.
 *
 * Memory follows `weivel/services.h`: a tree a consumer hands in is borrowed for the call only; a tree a provider
 * answers is the provider's until the consumer hands it to the definition's release for it, which frees every node and
 * text under `root` and leaves the tree empty with its `size` kept, a no-op for NULL, an empty tree or one below its
 * first shape.
 */

#include <stddef.h>
#include <stdint.h>

/** How deep a tree may nest below its root, which also bounds every walk over a tree that points into itself. */
#define WEIVEL_STANDARD_VALUE_DEPTH_LIMIT 1000

/** The kinds are append-only: a published kind keeps its number, and none is reused. */
typedef enum weivel_standard_value_kind {
    /** No value at all, as an answer that has none holds: only ever a tree's root, never a member or an item. */
    WEIVEL_STANDARD_VALUE_NONE = 0,
    WEIVEL_STANDARD_VALUE_NULL = 1,
    WEIVEL_STANDARD_VALUE_BOOLEAN = 2,
    WEIVEL_STANDARD_VALUE_NUMBER = 3,
    WEIVEL_STANDARD_VALUE_TEXT = 4,
    WEIVEL_STANDARD_VALUE_LIST = 5,
    WEIVEL_STANDARD_VALUE_RECORD = 6
} weivel_standard_value_kind;

typedef struct weivel_standard_entry weivel_standard_entry;

/**
 * One node: `kind` says which fields hold it, and every other field is zero. `boolean` is 0 or 1; `number` a finite
 * double, so an integer is exact only up to 2^53 in magnitude; `text` well-formed UTF-8 of `length` bytes, none of
 * them NUL, followed by a NUL; a list `count` nodes at `items`; a record `count` entries at `entries`, in order, each
 * key once. Every node is reached exactly once from its root, so no node is shared by two parents and none lies on a
 * cycle. A node sits inside arrays, so it carries no size and never grows: a later kind none of these fields can hold
 * arrives as a new definition.
 */
typedef struct weivel_standard_value {
    uint32_t kind;
    uint32_t boolean;
    double number;
    const char *text;
    size_t length;
    const struct weivel_standard_value *items;
    const weivel_standard_entry *entries;
    size_t count;
} weivel_standard_value;

/** One member of a record: a key under the same rules as a text, and its value, never of kind NONE. */
struct weivel_standard_entry {
    const char *key;
    size_t key_length;
    weivel_standard_value value;
};

/**
 * A tree a provider built, owning every node and text under `root` until the definition's release for it. An empty
 * tree has its `size` set, `root` of kind NONE and NULL storage, as WEIVEL_INIT leaves it. First shape ends at
 * `storage`.
 */
typedef struct weivel_standard_value_tree {
    uint32_t size;
    weivel_standard_value root;
    void *storage;
} weivel_standard_value_tree;

#endif
