/** First shape ends at `context`. */
typedef struct probe_calls {
    uint32_t size;
    void *context;
    void (*callbacks[2])(void *context);
} probe_calls;
