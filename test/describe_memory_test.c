#include <stdio.h>

#include "allocation.h"
#include "check.h"
#include "reader.h"

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))
#define FIXTURE(name) WEIVEL_DESCRIBE_FIXTURES "/" name

static void release_answered(describe_outcome outcome, describe_model *model, describe_refusal *refusal) {
    if (outcome == DESCRIBE_DONE) {
        describe_model_release(model);
    }
    if (outcome == DESCRIBE_REFUSED) {
        describe_refusal_release(refusal);
    }
}

/** Fails each allocation of one read in turn, until a read allocates nothing that fails. */
static void reads_through_every_failed_allocation(const char *const *paths, size_t count,
                                                  describe_outcome expected) {
    describe_names names = describe_names_of_the_standard();
    uint64_t nth;
    for (nth = 1;; nth++) {
        describe_model model;
        describe_refusal refusal;
        describe_outcome outcome;
        int failed;
        weivel_standard_memory_fail_at(nth);
        outcome = describe_read(&model, paths, count, &names, &refusal);
        failed = weivel_standard_memory_failed();
        weivel_standard_memory_fail_at(0);
        if (failed) {
            CHECK(outcome == DESCRIBE_OUT_OF_MEMORY);
            CHECK(refusal.message == NULL && refusal.file == NULL);
        } else {
            CHECK(outcome == expected);
        }
        release_answered(outcome, &model, &refusal);
        if (weivel_standard_memory_live_blocks() != 0) {
            fprintf(stderr, "%s: %d blocks live after failing allocation %u\n", paths[count - 1],
                    (int)weivel_standard_memory_live_blocks(), (unsigned)nth);
            check_failures++;
            return;
        }
        if (!failed) {
            return;
        }
    }
}

static void reaches_through_every_failed_allocation(void) {
    const char *paths[] = {WEIVEL_DESCRIBE_SERVICES_HEADER};
    describe_names names = describe_names_of_the_standard();
    describe_model model;
    describe_refusal refusal;
    uint64_t nth;
    CHECK(describe_read(&model, paths, 1, &names, &refusal) == DESCRIBE_DONE);
    for (nth = 1;; nth++) {
        describe_reach reach;
        describe_outcome outcome;
        int failed;
        weivel_standard_memory_fail_at(nth);
        outcome = describe_reach_of(&model, "weivel_standard_config", &reach, &refusal);
        failed = weivel_standard_memory_failed();
        weivel_standard_memory_fail_at(0);
        CHECK(outcome == (failed ? DESCRIBE_OUT_OF_MEMORY : DESCRIBE_DONE));
        describe_reach_release(&reach);
        if (!failed) {
            break;
        }
    }
    describe_model_release(&model);
    CHECK(weivel_standard_memory_live_blocks() == 0);
}

int main(void) {
    const char *services[] = {WEIVEL_DESCRIBE_SERVICES_HEADER};
    const char *every_form[] = {WEIVEL_DESCRIBE_SERVICES_HEADER, FIXTURE("every_form.h")};
    const char *clauses[] = {WEIVEL_DESCRIBE_SERVICES_HEADER, FIXTURE("clauses.h")};
    const char *refused[] = {FIXTURE("refused/answer_unreleased.h")};
    const char *unread[] = {FIXTURE("refused/missing.h")};
    reads_through_every_failed_allocation(services, COUNT_OF(services), DESCRIBE_DONE);
    reads_through_every_failed_allocation(every_form, COUNT_OF(every_form), DESCRIBE_DONE);
    reads_through_every_failed_allocation(clauses, COUNT_OF(clauses), DESCRIBE_DONE);
    reads_through_every_failed_allocation(refused, COUNT_OF(refused), DESCRIBE_REFUSED);
    reads_through_every_failed_allocation(unread, COUNT_OF(unread), DESCRIBE_REFUSED);
    reaches_through_every_failed_allocation();
    return check_summary("describe_memory_test");
}
