/**
 * Structs that point at themselves twice and at each other in rings, so walking what an answer holds must visit each
 * struct once rather than every path through them.
 */

typedef struct probe_ring_00 {
    const probe_ring_00 *left;
    const probe_ring_00 *right;
    const probe_ring_01 *next;
} probe_ring_00;

typedef struct probe_ring_01 {
    const probe_ring_01 *left;
    const probe_ring_01 *right;
    const probe_ring_02 *next;
} probe_ring_01;

typedef struct probe_ring_02 {
    const probe_ring_02 *left;
    const probe_ring_02 *right;
    const probe_ring_03 *next;
} probe_ring_02;

typedef struct probe_ring_03 {
    const probe_ring_03 *left;
    const probe_ring_03 *right;
    const probe_ring_04 *next;
} probe_ring_03;

typedef struct probe_ring_04 {
    const probe_ring_04 *left;
    const probe_ring_04 *right;
    const probe_ring_05 *next;
} probe_ring_04;

typedef struct probe_ring_05 {
    const probe_ring_05 *left;
    const probe_ring_05 *right;
    const probe_ring_06 *next;
} probe_ring_05;

typedef struct probe_ring_06 {
    const probe_ring_06 *left;
    const probe_ring_06 *right;
    const probe_ring_07 *next;
} probe_ring_06;

typedef struct probe_ring_07 {
    const probe_ring_07 *left;
    const probe_ring_07 *right;
    const probe_ring_08 *next;
} probe_ring_07;

typedef struct probe_ring_08 {
    const probe_ring_08 *left;
    const probe_ring_08 *right;
    const probe_ring_09 *next;
} probe_ring_08;

typedef struct probe_ring_09 {
    const probe_ring_09 *left;
    const probe_ring_09 *right;
    const probe_ring_10 *next;
} probe_ring_09;

typedef struct probe_ring_10 {
    const probe_ring_10 *left;
    const probe_ring_10 *right;
    const probe_ring_11 *next;
} probe_ring_10;

typedef struct probe_ring_11 {
    const probe_ring_11 *left;
    const probe_ring_11 *right;
    const probe_ring_12 *next;
} probe_ring_11;

typedef struct probe_ring_12 {
    const probe_ring_12 *left;
    const probe_ring_12 *right;
    const probe_ring_13 *next;
} probe_ring_12;

typedef struct probe_ring_13 {
    const probe_ring_13 *left;
    const probe_ring_13 *right;
    const probe_ring_14 *next;
} probe_ring_13;

typedef struct probe_ring_14 {
    const probe_ring_14 *left;
    const probe_ring_14 *right;
    const probe_ring_15 *next;
} probe_ring_14;

typedef struct probe_ring_15 {
    const probe_ring_15 *left;
    const probe_ring_15 *right;
    const probe_ring_16 *next;
} probe_ring_15;

typedef struct probe_ring_16 {
    const probe_ring_16 *left;
    const probe_ring_16 *right;
    const probe_ring_17 *next;
} probe_ring_16;

typedef struct probe_ring_17 {
    const probe_ring_17 *left;
    const probe_ring_17 *right;
    const probe_ring_18 *next;
} probe_ring_17;

typedef struct probe_ring_18 {
    const probe_ring_18 *left;
    const probe_ring_18 *right;
    const probe_ring_19 *next;
} probe_ring_18;

typedef struct probe_ring_19 {
    const probe_ring_19 *left;
    const probe_ring_19 *right;
    const probe_ring_20 *next;
} probe_ring_19;

typedef struct probe_ring_20 {
    const probe_ring_20 *left;
    const probe_ring_20 *right;
    const probe_ring_21 *next;
} probe_ring_20;

typedef struct probe_ring_21 {
    const probe_ring_21 *left;
    const probe_ring_21 *right;
    const probe_ring_22 *next;
} probe_ring_21;

typedef struct probe_ring_22 {
    const probe_ring_22 *left;
    const probe_ring_22 *right;
    const probe_ring_23 *next;
} probe_ring_22;

typedef struct probe_ring_23 {
    const probe_ring_23 *left;
    const probe_ring_23 *right;
    const probe_ring_24 *next;
} probe_ring_23;

typedef struct probe_ring_24 {
    const probe_ring_24 *left;
    const probe_ring_24 *right;
    const probe_ring_25 *next;
} probe_ring_24;

typedef struct probe_ring_25 {
    const probe_ring_25 *left;
    const probe_ring_25 *right;
    const probe_ring_26 *next;
} probe_ring_25;

typedef struct probe_ring_26 {
    const probe_ring_26 *left;
    const probe_ring_26 *right;
    const probe_ring_27 *next;
} probe_ring_26;

typedef struct probe_ring_27 {
    const probe_ring_27 *left;
    const probe_ring_27 *right;
    const probe_ring_28 *next;
} probe_ring_27;

typedef struct probe_ring_28 {
    const probe_ring_28 *left;
    const probe_ring_28 *right;
    const probe_ring_29 *next;
} probe_ring_28;

typedef struct probe_ring_29 {
    const probe_ring_29 *left;
    const probe_ring_29 *right;
    const probe_ring_00 *next;
} probe_ring_29;

typedef struct probe_far_00 {
    const probe_far_00 *left;
    const probe_far_00 *right;
    const probe_far_01 *next;
} probe_far_00;

typedef struct probe_far_01 {
    const probe_far_01 *left;
    const probe_far_01 *right;
    const probe_far_02 *next;
} probe_far_01;

typedef struct probe_far_02 {
    const probe_far_00 *back;
    void *storage;
} probe_far_02;

/** Holds storage itself and points at a struct that points back at it. */
typedef struct probe_loop_holder {
    const probe_loop_back *back;
    void *storage;
} probe_loop_holder;

/** Holds no storage itself, only through the struct it points back at. */
typedef struct probe_loop_back {
    const probe_loop_holder *holder;
} probe_loop_back;

/** The graph service. First shape ends at `release_loop_back`. */
typedef struct probe_graph {
    uint32_t size;
    void *context;
    weivel_status (*ring)(void *context, probe_ring_00 *ring);
    weivel_status (*far)(void *context, probe_far_00 *far);
    weivel_status (*holder)(void *context, probe_loop_holder *holder);
    weivel_status (*back)(void *context, probe_loop_back *back);
    void (*release_far)(void *context, probe_far_00 *far);
    void (*release_loop_holder)(void *context, probe_loop_holder *holder);
    void (*release_loop_back)(void *context, probe_loop_back *back);
} probe_graph;
