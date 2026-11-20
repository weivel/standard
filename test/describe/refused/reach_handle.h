/** First shape ends at `storage`. */
typedef struct weivel_standard_service_refusal {
    uint32_t size;
    const char *detail;
    const char *fix;
    void *storage;
} weivel_standard_service_refusal;

typedef struct probe_walk probe_walk;

/** First shape ends at `storage`. */
typedef struct probe_holder {
    uint32_t size;
    probe_walk *walk;
    void *storage;
} probe_holder;

/** First shape ends at `release_walk`. */
typedef struct probe_walker {
    uint32_t size;
    void *context;
    void (*release_walk)(void *context, probe_walk *walk);
} probe_walker;

/** First shape ends at `release_holder`. */
typedef struct probe_keeper {
    uint32_t size;
    void *context;
    weivel_status (*hold)(void *context, probe_holder *holder);
    void (*release_holder)(void *context, probe_holder *holder);
} probe_keeper;
