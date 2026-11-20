#include <stdio.h>
#include <string.h>

#include "allocation.h"
#include "check.h"
#include "reader.h"
#include "writer.h"

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

/** Fails each allocation of one write in turn, the definition's or a refused one's. */
static void writes_through_every_failed_allocation(const char *definition, describe_outcome expected) {
    const char *paths[] = {WEIVEL_DESCRIBE_SERVICES_HEADER, FIXTURE("every_form.h")};
    const char *includes[] = {"weivel/services.h", "every_form.h"};
    describe_names names = describe_names_of_the_standard();
    describe_service service;
    describe_model model;
    describe_refusal refusal;
    uint64_t nth;
    memset(&service, 0, sizeof service);
    service.definition = definition;
    service.symbol = "probe_description_of";
    service.prefix = "probe_";
    service.includes = includes;
    service.include_count = COUNT_OF(includes);
    CHECK(describe_read(&model, paths, COUNT_OF(paths), &names, &refusal) == DESCRIBE_DONE);
    for (nth = 1;; nth++) {
        describe_text source;
        describe_outcome outcome;
        int failed;
        weivel_standard_memory_fail_at(nth);
        outcome = describe_write(&model, &service, &source, &refusal);
        failed = weivel_standard_memory_failed();
        weivel_standard_memory_fail_at(0);
        if (failed) {
            CHECK(outcome == DESCRIBE_OUT_OF_MEMORY);
            CHECK(source.bytes == NULL && refusal.message == NULL && refusal.file == NULL);
        } else {
            CHECK(outcome == expected);
        }
        describe_text_release(&source);
        if (outcome == DESCRIBE_REFUSED) {
            describe_refusal_release(&refusal);
        }
        if (!failed) {
            break;
        }
    }
    describe_model_release(&model);
    if (weivel_standard_memory_live_blocks() != 0) {
        fprintf(stderr, "writing %s: %d blocks live\n", definition, (int)weivel_standard_memory_live_blocks());
        check_failures++;
    }
}

int main(void) {
    const char *services[] = {WEIVEL_DESCRIBE_SERVICES_HEADER};
    const char *every_form[] = {WEIVEL_DESCRIBE_SERVICES_HEADER, FIXTURE("every_form.h")};
    const char *clauses[] = {WEIVEL_DESCRIBE_SERVICES_HEADER, FIXTURE("clauses.h")};
    const char *owners[] = {FIXTURE("plurals_and_owners.h")};
    const char *cycles[] = {FIXTURE("cycles.h")};
    const char *refused[] = {FIXTURE("refused/answer_unreleased.h")};
    const char *unread[] = {FIXTURE("refused/missing.h")};
    reads_through_every_failed_allocation(services, COUNT_OF(services), DESCRIBE_DONE);
    reads_through_every_failed_allocation(every_form, COUNT_OF(every_form), DESCRIBE_DONE);
    reads_through_every_failed_allocation(clauses, COUNT_OF(clauses), DESCRIBE_DONE);
    reads_through_every_failed_allocation(owners, COUNT_OF(owners), DESCRIBE_DONE);
    reads_through_every_failed_allocation(cycles, COUNT_OF(cycles), DESCRIBE_DONE);
    reads_through_every_failed_allocation(refused, COUNT_OF(refused), DESCRIBE_REFUSED);
    reads_through_every_failed_allocation(unread, COUNT_OF(unread), DESCRIBE_REFUSED);
    reaches_through_every_failed_allocation();
    writes_through_every_failed_allocation("probe_definition", DESCRIBE_DONE);
    writes_through_every_failed_allocation("probe_item", DESCRIBE_REFUSED);
    return check_summary("describe_memory_test");
}
