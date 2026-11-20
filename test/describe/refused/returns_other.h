/** First shape ends at `run`. */
typedef struct probe_service {
    uint32_t size;
    void *context;
    uint32_t (*run)(void *context);
} probe_service;
