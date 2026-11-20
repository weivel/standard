/** First shape ends at `storage`. */
typedef struct probe_answer {
    uint32_t size;
    void *storage;
} probe_answer;

/** First shape ends at `release_answer`. */
typedef struct probe_service {
    uint32_t size;
    void *context;
    weivel_status (*run)(void *context, probe_answer *answer);
    /** @blocking it waits for the answer's storage to be free. */
    void (*release_answer)(void *context, probe_answer *answer);
} probe_service;
