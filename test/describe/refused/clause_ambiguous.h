typedef enum probe_mode { PROBE_FAST } probe_mode;

/** Runs in a probe_mode. */
typedef struct probe_run {
    uint32_t mode;
    uint32_t speed;
} probe_run;
