/** First shape ends at `storage`. */
typedef struct weivel_standard_service_refusal {
    uint32_t size;
    const char *detail;
    const char *fix;
    void *storage;
} weivel_standard_service_refusal;

typedef struct probe_walk probe_walk;

/** First shape ends at `release_walk`. */
typedef struct probe_first {
    uint32_t size;
    void *context;
    void (*release_walk)(void *context, probe_walk *walk);
} probe_first;

/** First shape ends at `free_walk`. */
typedef struct probe_second {
    uint32_t size;
    void *context;
    void (*free_walk)(void *context, probe_walk *walk);
} probe_second;
