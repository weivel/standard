#ifndef WEIVEL_STANDARD_TEST_DESCRIBED_H
#define WEIVEL_STANDARD_TEST_DESCRIBED_H

/**
 * @file
 * A neutral C library of two definition services, described by weivel-describe, that every weivel language binds
 * generically in its tests: the mirror, which answers what it is handed in every member kind, and the walker, which
 * answers handles, lends a counter, a definition of its own, and declares one operation that blocks. It is a test
 * target only, built by a plain configure of weivel/standard, never linked into the standard and exported by nothing
 * the standard ships.
 *
 * Every operation answers from its input alone, so a binding's round trip is checkable value for value. The two
 * definitions follow `weivel/services.h`'s conventions: an answer is filled on OK alone, MALFORMED is answered for a
 * NULL required pointer or an answer below its first shape or still holding storage, and every answer that owns
 * storage, every handle and every refusal is freed by the definition's release for its kind. A refusal always carries
 * the detail DESCRIBED_REFUSAL_DETAIL and the fix DESCRIBED_REFUSAL_FIX.
 */

#include <stddef.h>
#include <stdint.h>

#include "weivel/description.h"
#include "weivel/services.h"
#include "weivel/value.h"

#if defined(_WIN32)
#if defined(DESCRIBED_BUILDING)
#define DESCRIBED_API __declspec(dllexport)
#else
#define DESCRIBED_API __declspec(dllimport)
#endif
#else
#define DESCRIBED_API __attribute__((visibility("default")))
#endif

/** The detail of every refusal the library answers. */
#define DESCRIBED_REFUSAL_DETAIL "described refused on purpose"
/** The fix of every refusal the library answers. */
#define DESCRIBED_REFUSAL_FIX "pass anything but \"refuse\""
/** Where the counter the walker lends starts, which its own context holds. */
#define DESCRIBED_COUNTER_START 1000

#ifdef __cplusplus
extern "C" {
#endif

/** An open walk the walker answers, opaque to everyone but the library. */
typedef struct described_cursor described_cursor;

/** A shade, bound to its member by the member's own doc. */
typedef enum described_shade { DESCRIBED_SHADE_LIGHT = 1, DESCRIBED_SHADE_DARK = 2 } described_shade;

/** A tier, bound to its member by the doc of the struct holding it. */
typedef enum described_tier { DESCRIBED_TIER_LOW = 0, DESCRIBED_TIER_HIGH = 7 } described_tier;

/** One point: no size, and it never grows. */
typedef struct described_point {
    const char *label;
    int64_t x;
    double y;
} described_point;

/**
 * A member of every kind a binding marshals, as the mirror reads and answers it: `tier` is a described_tier. A NULL
 * text, pointer or array, an array's count 0 with it, is none. First shape ends at `value_count`.
 */
typedef struct described_sample {
    uint32_t size;
    /** A described_shade. */
    uint32_t shade;
    uint32_t tier;
    uint32_t flags;
    uint64_t big;
    int64_t delta;
    double ratio;
    const char *label;
    const char *const *tags;
    size_t tag_count;
    const uint8_t *bytes;
    size_t length;
    described_point origin;
    const described_point *best;
    const described_point *points;
    size_t point_count;
    const uint32_t *numbers;
    size_t number_count;
    weivel_standard_value held;
    const weivel_standard_value *extra;
    const weivel_standard_value *values;
    size_t value_count;
} described_sample;

/**
 * What `reflect` answers: a copy of the sample, every text, array and value under it owned by `storage`. First shape
 * ends at `storage`.
 */
typedef struct described_reflection {
    uint32_t size;
    described_sample sample;
    void *storage;
} described_reflection;

/**
 * How many releases of each kind have freed something since the library was loaded, so a binding can prove it
 * releases every answer exactly once: a release handed an empty struct or NULL counts nothing. It owns nothing, so
 * nothing releases it. First shape ends at `cursors`.
 *
 * @implNote The counts are kept without synchronisation, so they may be read only while no other thread releases.
 */
typedef struct described_releases {
    uint32_t size;
    uint64_t reflections;
    uint64_t trees;
    uint64_t refusals;
    uint64_t cursors;
} described_releases;

/** The mirror, the library's first service. First shape ends at `release_refusal`. */
typedef struct described_mirror {
    uint32_t size;
    void *context;
    /**
     * Copies the sample, every member of it, into the answer, `size` included as the library compiled it.
     *
     * @param sample read during the call only. Required.
     * @param reflection the consumer's empty answer. Required.
     * @param refusal the consumer's empty refusal, or NULL for none.
     * @return OK; MALFORMED for a sample below its first shape, a count beside a NULL array, a value nested deeper
     * than WEIVEL_STANDARD_VALUE_DEPTH_LIMIT, a value of kind NONE anywhere below the root of `held`, `extra` or an
     * element of `values`, or a text or key that is NULL with a nonzero length or whose length is SIZE_MAX; REFUSED
     * when the sample's `label` is "refuse".
     */
    weivel_status (*reflect)(void *context, const described_sample *sample, described_reflection *reflection,
                             weivel_standard_service_refusal *refusal);
    /**
     * Builds a value tree from the sample: a record of, in order, `label` (a text, or null for none), `tags` (a list
     * of texts, a NULL tag as null), `ratio` (a number) and `held` (a copy of the sample's held value, null when it
     * holds none).
     *
     * @param sample read during the call only. Required.
     * @param tree the consumer's empty answer. Required.
     * @param refusal the consumer's empty refusal, or NULL for none.
     * @return OK; MALFORMED as `reflect` answers it.
     */
    weivel_status (*tree)(void *context, const described_sample *sample, weivel_standard_value_tree *tree,
                          weivel_standard_service_refusal *refusal);
    /**
     * Looks a key up in the library's fixed table, "alpha" naming "the first letter" and "omega" "the last letter".
     *
     * @param key Required.
     * @param found answers the text the key names, which the library keeps, so nothing releases it. Required.
     * @param refusal the consumer's empty refusal, or NULL for none.
     * @return OK; NONE for a key the table does not hold; MALFORMED for a NULL `key` or `found`, or a refusal below
     * its first shape or still holding storage; REFUSED for the key "refuse".
     */
    weivel_status (*find)(void *context, const char *key, const char **found,
                          weivel_standard_service_refusal *refusal);
    /**
     * Sums bytes.
     *
     * @param bytes `length` bytes, any bytes at all, read during the call only; NULL only when `length` is 0.
     * @param sum answers the sum of the bytes, each read as unsigned. Required.
     */
    weivel_status (*measure)(void *context, const uint8_t *bytes, size_t length, uint64_t *sum);
    /**
     * Negates a number.
     *
     * @param negated answers `-number`. Required.
     * @return OK; MALFORMED for a NULL `negated`, or for INT64_MIN, whose negation no int64_t holds.
     */
    weivel_status (*negate)(void *context, int64_t number, int64_t *negated);
    /**
     * Halves a number.
     *
     * @param half answers `number / 2`. Required.
     * @return OK; MALFORMED for a NULL `half`.
     */
    weivel_status (*halve)(void *context, double number, double *half);
    /**
     * Inverts every bit of a flag word.
     *
     * @param inverted answers `~flags`. Required.
     * @return OK; MALFORMED for a NULL `inverted`.
     */
    weivel_status (*invert)(void *context, uint32_t flags, uint32_t *inverted);
    /**
     * The other shade.
     *
     * @param shade a described_shade.
     * @param other answers a described_shade, DARK for LIGHT and LIGHT for DARK. Required.
     * @return OK; MALFORMED for a NULL `other` or a `shade` that is no described_shade.
     */
    weivel_status (*swap_shade)(void *context, uint32_t shade, uint32_t *other);
    /**
     * Grows an amount by one.
     *
     * @param grown answers `amount + 1`. Required.
     * @return OK; MALFORMED for a NULL `grown`, or for SIZE_MAX, which no size_t holds one more than.
     */
    weivel_status (*grow)(void *context, size_t amount, size_t *grown);
    /**
     * Counts the nodes of a value, itself included.
     *
     * @param value read during the call only. Required.
     * @param nodes answers how many nodes the value holds. Required.
     * @return OK; MALFORMED for a NULL `value` or `nodes`, a count beside NULL items or entries, or a value nested
     * deeper than WEIVEL_STANDARD_VALUE_DEPTH_LIMIT.
     */
    weivel_status (*weigh)(void *context, const weivel_standard_value *value, uint64_t *nodes);
    /**
     * How many releases of each kind, of both services, have freed something.
     *
     * @param releases the consumer's empty answer. Required.
     */
    weivel_status (*count_releases)(void *context, described_releases *releases);
    void (*release_reflection)(void *context, described_reflection *reflection);
    void (*release_tree)(void *context, weivel_standard_value_tree *tree);
    void (*release_refusal)(void *context, weivel_standard_service_refusal *refusal);
} described_mirror;

/** Where to look from an open walk: its start, moved by `skip` and by every step. First shape ends at `step_count`. */
typedef struct described_visit {
    uint32_t size;
    const described_cursor *cursor;
    uint64_t skip;
    const uint64_t *steps;
    size_t step_count;
} described_visit;

/**
 * The counter the walker lends, one static definition with a context of its own holding DESCRIBED_COUNTER_START. First
 * shape ends at `add`.
 */
typedef struct described_counter {
    uint32_t size;
    void *context;
    /**
     * Adds to where the counter starts.
     *
     * @param total answers DESCRIBED_COUNTER_START plus `amount`. Required.
     * @return OK; MALFORMED for a NULL `total`, or an `amount` no uint64_t holds DESCRIBED_COUNTER_START more than.
     */
    weivel_status (*add)(void *context, uint64_t amount, uint64_t *total);
} described_counter;

/** What `lend` answers: the walker's counter, living until the library is unloaded. First shape ends at `counter`. */
typedef struct described_loan {
    uint32_t size;
    const described_counter *counter;
} described_loan;

/**
 * The walker, the library's second service. First shape ends at `release_refusal`, and `wait` and `lend` follow it, as
 * a definition grows.
 */
typedef struct described_walker {
    uint32_t size;
    void *context;
    /**
     * Opens a walk starting at `start`.
     *
     * @param label Required.
     * @param cursor the consumer's empty handle, freed by `release_cursor`. Required.
     * @param refusal the consumer's empty refusal, or NULL for none.
     * @return OK; REFUSED when `label` is "refuse".
     */
    weivel_status (*open)(void *context, const char *label, uint64_t start, described_cursor **cursor,
                          weivel_standard_service_refusal *refusal);
    /**
     * Where a walk started.
     *
     * @param cursor read during the call only. Required.
     * @param start answers the start the walk was opened with. Required.
     */
    weivel_status (*use)(void *context, const described_cursor *cursor, uint64_t *start);
    /**
     * Where a visit lands: its cursor's start, plus its `skip`, plus every step.
     *
     * @param visit read during the call only. Required, and so is its cursor.
     * @param position answers where the visit lands. Required.
     * @return OK; MALFORMED for a NULL `visit`, cursor or `position`, a visit below its first shape, or a nonzero
     * `step_count` beside NULL `steps`.
     */
    weivel_status (*visit)(void *context, const described_visit *visit, uint64_t *position);
    void (*release_cursor)(void *context, described_cursor *cursor);
    void (*release_refusal)(void *context, weivel_standard_service_refusal *refusal);
    /**
     * Answers how many ticks it was asked to wait, at once.
     *
     * @blocking it stands for an operation that waits, so a binding proves it calls one away from its calling thread.
     * @param waited answers `ticks`. Required.
     * @return OK; MALFORMED for a NULL `waited`.
     */
    weivel_status (*wait)(void *context, uint64_t ticks, uint64_t *waited);
    /**
     * Lends the walker's counter.
     *
     * @param loan the consumer's empty answer, which owns nothing, so nothing releases it. Required.
     * @return OK; MALFORMED for a NULL `loan` or a loan below its first shape.
     */
    weivel_status (*lend)(void *context, described_loan *loan);
} described_walker;

/** Answers the mirror, one static described_mirror living until the library is unloaded. */
DESCRIBED_API const void *described_mirror_definition(void);

/** Answers the walker, one static described_walker living until the library is unloaded. */
DESCRIBED_API const void *described_walker_definition(void);

/**
 * Answers a mirror that does not offer `halve`, holding NULL there and every other operation the mirror's own, so a
 * binding shows what a call of an operation its provider does not offer does. It is described by the mirror's
 * description.
 */
DESCRIBED_API const void *described_mirror_without_halve_definition(void);

/** The mirror's description, written by weivel-describe. */
DESCRIBED_API const weivel_standard_description *described_mirror_description(void);

/** The walker's description, written by weivel-describe. */
DESCRIBED_API const weivel_standard_description *described_walker_description(void);

#ifdef __cplusplus
}
#endif

#endif
