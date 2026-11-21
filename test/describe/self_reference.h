/**
 * A struct pointing at its own type, spelled `const struct <tag> *` as C requires inside the struct's own body, beside
 * the same struct spelled by its typedef name, which C does not allow there: both must describe alike.
 */

/** A node spelled as C compiles it. */
typedef struct probe_spelled {
    const char *label;
    const struct probe_spelled *next;
    const struct probe_spelled *content;
    size_t content_count;
} probe_spelled;

/** The same node spelled by its typedef name. */
typedef struct probe_named {
    const char *label;
    const probe_named *next;
    const probe_named *content;
    size_t content_count;
} probe_named;

/** What `copy` answers. First shape ends at `storage`. */
typedef struct probe_tree {
    uint32_t size;
    probe_spelled root;
    void *storage;
} probe_tree;

/** The tree service. First shape ends at `release_tree`. */
typedef struct probe_trees {
    uint32_t size;
    void *context;
    weivel_status (*copy)(void *context, const probe_spelled *spelled, const probe_named *named, probe_tree *tree);
    void (*release_tree)(void *context, probe_tree *tree);
} probe_trees;
