typedef enum probe_mode { PROBE_FAST = 1, PROBE_SLOW = 2 } probe_mode;

typedef struct probe_run {
    /** The probe_mode it ran in, by name. */
    const char *mode;
} probe_run;
