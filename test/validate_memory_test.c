#include "allocation.h"
#include "check.h"
#include "weivel/standard.h"

static const char *const MANY_FAULTS =
    "{\"importName\": \"a\", \"api\": \"one\", \"entry\": 5, \"targets\": [1],"
    " \"capabilities\": {\"provides\": [\"screens\", \"screens\"]},"
    " \"services\": {\"provides\": [\"a:x\", {\"id\": \"a:x\"}, \"routing\", {\"attributes\": {\"x\": \"y\"}}],"
    "   \"consumes\": [{\"id\": \"clock\", \"provider\": \"Forge:1\", \"optional\": 1}]},"
    " \"permissions\": [\"net\", \"net\"], \"dependencies\": [{\"importName\": \"r\"}, {\"id\": 5}]}";

static const char *const VALID =
    "{\"importName\": \"a\", \"api\": 2, \"entry\": \"lib/a.so\", \"capabilities\": {\"provides\": [\"screens\"]},"
    " \"services\": {\"provides\": [\"a:x\", \"accounts\"], \"consumes\": [{\"id\": \"clock\"}]}}";

static const char *const MALFORMED = "{\"importName\": ";

typedef weivel_status (*answering)(const char *text, weivel_standard_validation *validation);

static const char *const WELL_KNOWN_IDS[] = {"accounts", "clock"};

static weivel_status validate_with_list(const char *text, weivel_standard_validation *validation) {
    weivel_strings well_known = {WELL_KNOWN_IDS, 2};
    weivel_standard_validation_options options = WEIVEL_INIT(weivel_standard_validation_options);
    options.well_known_services = &well_known;
    return weivel_standard_validate(text, strlen(text), &options, validation);
}

static weivel_status load_and_release(const char *text, weivel_standard_validation *validation) {
    weivel_standard_manifest *manifest = NULL;
    weivel_status status = weivel_standard_load_manifest(text, strlen(text), NULL, &manifest, validation);
    CHECK((status == WEIVEL_STATUS_OK) == (manifest != NULL));
    weivel_standard_manifest_release(manifest);
    return status;
}

static int is_empty(const weivel_standard_validation *validation) {
    return validation->paths.count == 0 && validation->paths.items == NULL && validation->storage == NULL &&
           validation->refusal.code == WEIVEL_STANDARD_REFUSAL_NONE && validation->refusal.storage == NULL;
}

/** Fails each allocation in turn until the answer needs no more, and checks every failure leaves nothing behind. */
static void every_failed_allocation_leaves_nothing(answering answer, const char *text, weivel_status settled) {
    uint64_t nth;
    for (nth = 1; nth < 100000; nth++) {
        weivel_standard_validation validation = WEIVEL_INIT(weivel_standard_validation);
        weivel_status status;
        int failed;
        weivel_standard_memory_fail_at(nth);
        status = answer(text, &validation);
        failed = weivel_standard_memory_failed();
        weivel_standard_memory_fail_at(0);
        if (!failed) {
            CHECK(status == settled);
            weivel_standard_validation_release(&validation);
            CHECK(weivel_standard_memory_live_blocks() == 0);
            CHECK(nth > 1);
            return;
        }
        if (status != WEIVEL_STATUS_OUT_OF_MEMORY) {
            fprintf(stderr, "failing allocation %llu answered %d\n", (unsigned long long)nth, (int)status);
        }
        CHECK(status == WEIVEL_STATUS_OUT_OF_MEMORY);
        CHECK(is_empty(&validation));
        CHECK(weivel_standard_memory_live_blocks() == 0);
    }
    CHECK(!"the validation never settled");
}

static void release_frees_everything(void) {
    weivel_standard_validation validation = WEIVEL_INIT(weivel_standard_validation);
    CHECK(weivel_standard_memory_live_blocks() == 0);
    CHECK(validate_with_list(MANY_FAULTS, &validation) == WEIVEL_STATUS_REFUSED);
    CHECK(validation.paths.count > 10);
    CHECK(weivel_standard_memory_live_blocks() > 0);
    weivel_standard_validation_release(&validation);
    CHECK(weivel_standard_memory_live_blocks() == 0);
}

int main(void) {
    release_frees_everything();
    every_failed_allocation_leaves_nothing(validate_with_list, MANY_FAULTS, WEIVEL_STATUS_REFUSED);
    every_failed_allocation_leaves_nothing(validate_with_list, VALID, WEIVEL_STATUS_OK);
    every_failed_allocation_leaves_nothing(validate_with_list, MALFORMED, WEIVEL_STATUS_REFUSED);
    every_failed_allocation_leaves_nothing(load_and_release, VALID, WEIVEL_STATUS_OK);
    every_failed_allocation_leaves_nothing(load_and_release, "{\"importName\": \"a\", \"api\": 5000000000}",
                                           WEIVEL_STATUS_REFUSED);
    return check_summary("validate_memory_test");
}
