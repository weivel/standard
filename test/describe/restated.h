/** A service whose library restates the status, refusal and value types. First shape ends at `release_refusal`. */
typedef struct probe_restated {
    uint32_t size;
    void *context;
    probe_status (*ask)(void *context, const probe_value *question, probe_tree *tree, probe_reply *reply,
                        probe_refusal *refusal);
    void (*release_tree)(void *context, probe_tree *tree);
    void (*release_reply)(void *context, probe_reply *reply);
    void (*release_refusal)(void *context, probe_refusal *refusal);
} probe_restated;
