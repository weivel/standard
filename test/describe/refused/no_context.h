/** First shape ends at `run`. */
typedef struct probe_service {
    uint32_t size;
    void *context;
    weivel_status (*run)(void *state);
} probe_service;
