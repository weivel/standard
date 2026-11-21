/**
 * A status type that is an enum, read with `probe_status` as the status: a doc naming the status an operation
 * answers names a role and binds nothing, where a doc naming any other enum must bind it.
 */

typedef enum probe_status { PROBE_STATUS_OK = 0, PROBE_STATUS_REFUSED = 1 } probe_status;

/** A job, which a run answers a probe_status for. */
typedef struct probe_job {
    /** Its label, never a probe_status. */
    const char *label;
} probe_job;

/** The job service, each operation answering a probe_status. First shape ends at `run`. */
typedef struct probe_jobs {
    uint32_t size;
    void *context;
    /**
     * Runs it, answering a probe_status.
     *
     * @param job read during the call only, answered for with a probe_status. Required.
     */
    probe_status (*run)(void *context, const probe_job *job);
} probe_jobs;
