/** First shape ends at `release_refusal`. */
typedef struct probe_service {
    uint32_t size;
    void *context;
    weivel_status (*run)(void *context, weivel_standard_service_refusal *refusal);
    void (*release_refusal)(void *context, weivel_standard_service_refusal *refusal);
} probe_service;
