typedef enum probe_status { PROBE_STATUS_OK = 0, PROBE_STATUS_REFUSED = 1 } probe_status;

typedef enum probe_mode { PROBE_FAST = 1, PROBE_SLOW = 2 } probe_mode;

/** The job service. First shape ends at `run`. */
typedef struct probe_jobs {
    uint32_t size;
    void *context;
    /** Runs it in a probe_mode, answering a probe_status. */
    probe_status (*run)(void *context, const char *label);
} probe_jobs;
