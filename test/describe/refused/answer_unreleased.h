/** First shape ends at `storage`. */
typedef struct weivel_standard_service_refusal {
    uint32_t size;
    const char *detail;
    const char *fix;
    void *storage;
} weivel_standard_service_refusal;

/** First shape ends at `storage`. */
typedef struct probe_answer {
    uint32_t size;
    void *storage;
} probe_answer;

/** First shape ends at `release_refusal`. */
typedef struct probe_service {
    uint32_t size;
    void *context;
    weivel_status (*run)(void *context, probe_answer *answer, weivel_standard_service_refusal *refusal);
    void (*release_refusal)(void *context, weivel_standard_service_refusal *refusal);
} probe_service;
