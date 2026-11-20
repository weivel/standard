typedef struct probe_item {
    uint64_t weight;
} probe_item;

/** First shape ends at `run`. */
typedef struct probe_service {
    uint32_t size;
    void *context;
    weivel_status (*run)(void *context, probe_item item);
} probe_service;
