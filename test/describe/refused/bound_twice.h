typedef enum probe_mode { PROBE_FAST } probe_mode;
typedef enum probe_level { PROBE_LOW } probe_level;

/** `mode` is a probe_level. */
typedef struct probe_run {
    /** A probe_mode. */
    uint32_t mode;
} probe_run;
