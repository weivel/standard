typedef struct probe_walk probe_walk;

typedef struct probe_entry {
    const char *key;
} probe_entry;

/** Counted by `y` plurals, and a handle pointed at as const. First shape ends at `walk`. */
typedef struct probe_index {
    uint32_t size;
    const probe_entry *entries;
    size_t entry_count;
    const char *const *libraries;
    size_t library_count;
    const probe_walk *walk;
} probe_index;

/** An answer that owns nothing, so nothing releases it. First shape ends at `total`. */
typedef struct probe_tally {
    uint32_t size;
    uint64_t total;
} probe_tally;

/** An answer that owns nothing and still has a release. First shape ends at `total`. */
typedef struct probe_count {
    uint32_t size;
    uint64_t total;
} probe_count;

typedef struct probe_inner {
    const char *text;
    void *storage;
} probe_inner;

/** An answer whose storage is held by a struct it points at. First shape ends at `inner`. */
typedef struct probe_outer {
    uint32_t size;
    const probe_inner *inner;
} probe_outer;

/** The index service. First shape ends at `release_walk`. */
typedef struct probe_indexer {
    uint32_t size;
    void *context;
    weivel_status (*tally)(void *context, const probe_index *index, probe_tally *tally);
    weivel_status (*count)(void *context, probe_count *counted);
    weivel_status (*nest)(void *context, probe_outer *outer);
    weivel_status (*peek)(void *context, const probe_walk *walk, uint64_t *seen);
    void (*release_count)(void *context, probe_count *counted);
    void (*release_outer)(void *context, probe_outer *outer);
    void (*release_walk)(void *context, probe_walk *walk);
} probe_indexer;
