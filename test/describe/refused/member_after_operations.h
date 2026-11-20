/** First shape ends at `run`. */
typedef struct probe_service {
    uint32_t size;
    void *context;
    weivel_status (*run)(void *context);
    uint64_t extra;
} probe_service;
