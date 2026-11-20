typedef enum probe_mode { PROBE_FAST } probe_mode;
typedef enum probe_level { PROBE_LOW } probe_level;

/** Runs at a probe_mode and a probe_level. */
typedef struct probe_run {
    uint32_t mode;
    uint32_t level;
} probe_run;
