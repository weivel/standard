typedef enum probe_notice { PROBE_NOTICE_NONE = 0, PROBE_NOTICE_LATE = 1 } probe_notice;

typedef struct probe_report {
    /** Each a probe_notice. */
    const uint32_t *notices;
    size_t notice_count;
} probe_report;
