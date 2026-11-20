typedef struct probe_loop_holder {
    const probe_loop_back *back;
    void *storage;
} probe_loop_holder;

typedef struct probe_loop_back {
    const probe_loop_holder *holder;
} probe_loop_back;

/** First shape ends at `release_loop_holder`. */
typedef struct probe_graph {
    uint32_t size;
    void *context;
    weivel_status (*holder)(void *context, probe_loop_holder *holder);
    weivel_status (*back)(void *context, probe_loop_back *back);
    void (*release_loop_holder)(void *context, probe_loop_holder *holder);
} probe_graph;
