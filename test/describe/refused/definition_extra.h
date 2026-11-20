/** First shape ends at `run`. */
typedef struct probe_service {
    uint32_t size;
    void *context;
    uint64_t extra;
    weivel_status (*run)(void *context);
} probe_service;
