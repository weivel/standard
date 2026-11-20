/** Why a probe declined. First shape ends at `storage`. */
typedef struct probe_refusal {
    uint32_t size;
    const char *detail;
    const char *fix;
    void *storage;
} probe_refusal;

/** A probe's answer, declared in a header read after the one that uses it. First shape ends at `storage`. */
typedef struct probe_reply {
    uint32_t size;
    const probe_value *value;
    void *storage;
} probe_reply;
