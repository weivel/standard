/** First shape ends at `add`. */
typedef struct probe_counter {
    uint32_t size;
    void *context;
    weivel_status (*add)(void *context, uint64_t amount, uint64_t *total);
} probe_counter;

/** First shape ends at `use`. */
typedef struct probe_service {
    uint32_t size;
    void *context;
    weivel_status (*use)(void *context, const probe_counter *counter);
} probe_service;
