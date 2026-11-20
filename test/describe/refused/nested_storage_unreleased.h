typedef struct probe_inner {
    const char *text;
    void *storage;
} probe_inner;

/** First shape ends at `inner`. */
typedef struct probe_outer {
    uint32_t size;
    const probe_inner *inner;
} probe_outer;

/** First shape ends at `nest`. */
typedef struct probe_service {
    uint32_t size;
    void *context;
    weivel_status (*nest)(void *context, probe_outer *outer);
} probe_service;
