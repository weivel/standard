typedef enum probe_mode { PROBE_FAST } probe_mode;

/** `mode` and `speed` are each a probe_mode. */
typedef struct probe_run {
    uint32_t mode;
    uint32_t speed;
} probe_run;
