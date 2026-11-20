/** What `again` answers: the definition itself, read before C would let it be named. First shape ends at `ping`. */
typedef struct probe_echo {
    uint32_t size;
    const probe_ping *ping;
} probe_echo;

/** First shape ends at `again`. */
typedef struct probe_ping {
    uint32_t size;
    void *context;
    weivel_status (*again)(void *context, probe_echo *echo);
} probe_ping;
