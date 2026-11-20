typedef enum probe_level { PROBE_LOW = 0, PROBE_HIGH = 1 } probe_level;

typedef enum probe_color { PROBE_RED = 0, PROBE_BLUE = 1 } probe_color;

/**
 * A line: `start` is a time; `level` is a probe_level; `plain` 1 leaves it uncoloured. `start` is never a
 * probe_color, which a clause naming only members that hold no uint32_t binds to nothing.
 */
typedef struct probe_line {
    const char *start;
    uint32_t level;
    uint32_t plain;
} probe_line;

/** One choice: its key, and the probe_color it is drawn in, PROBE_RED meaning the default. */
typedef struct probe_choice {
    const char *key;
    uint32_t color;
} probe_choice;

/** Holds nothing an enum names, so the probe_level here binds nothing. */
typedef struct probe_label {
    const char *text;
} probe_label;

/** A sized struct whose only other uint32_t holds the probe_color. First shape ends at `color`. */
typedef struct probe_paint {
    uint32_t size;
    uint32_t color;
} probe_paint;

/** The paint service. First shape ends at `release_refusal`. */
typedef struct probe_painter {
    uint32_t size;
    void *context;
    /**
     * Paints in the probe_color `color` names.
     *
     * @param level a probe_level.
     * @param chosen answers the colour chosen.
     */
    weivel_status (*paint)(void *context, uint32_t level, uint32_t color, uint32_t *chosen,
                           weivel_standard_service_refusal *refusal);
    void (*release_refusal)(void *context, weivel_standard_service_refusal *refusal);
} probe_painter;
