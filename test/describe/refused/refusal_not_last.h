/** First shape ends at `storage`. */
typedef struct weivel_standard_service_refusal {
    uint32_t size;
    const char *detail;
    const char *fix;
    void *storage;
} weivel_standard_service_refusal;

/** First shape ends at `release_refusal`. */
typedef struct probe_service {
    uint32_t size;
    void *context;
    weivel_status (*run)(void *context, weivel_standard_service_refusal *refusal, uint64_t *count);
    void (*release_refusal)(void *context, weivel_standard_service_refusal *refusal);
} probe_service;
