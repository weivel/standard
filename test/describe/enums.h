/** Numbered every way the grammar reads. */
typedef enum probe_numbers {
    /** The first, numbered 0 by default. */
    PROBE_ZERO,
    PROBE_ONE,
    PROBE_TEN = 10,
    PROBE_ELEVEN,
    // a line comment between entries
    PROBE_HEX = 0x20,
    PROBE_LARGEST = 4294967295,
} probe_numbers;

typedef enum probe_empty { PROBE_ONLY } probe_empty;
