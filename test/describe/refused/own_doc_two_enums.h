typedef enum probe_mode { PROBE_FAST } probe_mode;
typedef enum probe_level { PROBE_LOW } probe_level;

typedef struct probe_run {
    /** A probe_mode or a probe_level. */
    uint32_t mode;
} probe_run;
