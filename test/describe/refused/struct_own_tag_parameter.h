/** First shape ends at `run`. */
typedef struct probe_service {
    uint32_t size;
    void *context;
    weivel_status (*run)(void *context, const struct probe_service *other);
} probe_service;
