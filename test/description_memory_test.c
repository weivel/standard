#include "allocation.h"
#include "check.h"
#include "description_fixture.h"

static void breaks_a_count(probe_description *probe) {
    probe->input[11].count = "probe_count";
}

static void breaks_a_name(probe_description *probe) {
    probe->operations[PROBE_READ].name = NULL;
}

static void breaks_a_nested_name(probe_description *probe) {
    probe->counter_types[0].name = NULL;
}

/** Fails each allocation in turn until a refused check needs no more, and checks every failure leaves nothing. */
static void every_failed_allocation_leaves_nothing(void (*breaks)(probe_description *probe)) {
    probe_description probe;
    uint64_t nth;
    probe_describe(&probe);
    breaks(&probe);
    for (nth = 1; nth < 1000; nth++) {
        weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
        weivel_status status;
        weivel_standard_memory_fail_at(nth);
        status = weivel_standard_description_check(&probe.description, &refusal);
        if (!weivel_standard_memory_failed()) {
            weivel_standard_memory_fail_at(0);
            CHECK(status == WEIVEL_STATUS_MALFORMED);
            weivel_standard_refusal_release(&refusal);
            CHECK(weivel_standard_memory_live_blocks() == 0);
            return;
        }
        weivel_standard_memory_fail_at(0);
        CHECK(status == WEIVEL_STATUS_OUT_OF_MEMORY);
        CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_NONE && refusal.storage == NULL);
        CHECK(weivel_standard_memory_live_blocks() == 0);
    }
    CHECK(0);
}

static void a_whole_description_allocates_nothing(void) {
    probe_description probe;
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    probe_describe(&probe);
    weivel_standard_memory_fail_at(1);
    CHECK(weivel_standard_description_check(&probe.description, &refusal) == WEIVEL_STATUS_OK);
    CHECK(!weivel_standard_memory_failed());
    weivel_standard_memory_fail_at(0);
}

/** Fails each allocation in turn until refusing a description that names no function needs no more. */
static void every_failed_allocation_refusing_an_answer_leaves_nothing(void) {
    static const int definition = 0;
    probe_description probe;
    uint64_t nth;
    probe_describe(&probe);
    for (nth = 1; nth < 100; nth++) {
        weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
        weivel_status status;
        weivel_standard_memory_fail_at(nth);
        status = weivel_standard_description_answers(&probe.description, &definition, &refusal);
        if (!weivel_standard_memory_failed()) {
            weivel_standard_memory_fail_at(0);
            CHECK(status == WEIVEL_STATUS_MALFORMED && refusal.code == WEIVEL_STANDARD_REFUSAL_MISSING_FIELD);
            weivel_standard_refusal_release(&refusal);
            CHECK(weivel_standard_memory_live_blocks() == 0);
            return;
        }
        weivel_standard_memory_fail_at(0);
        CHECK(status == WEIVEL_STATUS_OUT_OF_MEMORY);
        CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_NONE && refusal.storage == NULL);
        CHECK(weivel_standard_memory_live_blocks() == 0);
    }
    CHECK(0);
}

int main(void) {
    every_failed_allocation_leaves_nothing(breaks_a_count);
    every_failed_allocation_leaves_nothing(breaks_a_name);
    every_failed_allocation_leaves_nothing(breaks_a_nested_name);
    a_whole_description_allocates_nothing();
    every_failed_allocation_refusing_an_answer_leaves_nothing();
    return check_summary("description_memory_test");
}
