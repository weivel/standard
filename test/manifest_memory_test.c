#include "allocation.h"
#include "check.h"
#include "weivel/standard.h"

static const char *const FULL_MANIFEST =
    "{\"importName\": \"greeter\", \"api\": 2, \"entry\": \"dist/greeter.so\", \"targets\": [\"app.one\"],"
    " \"capabilities\": {\"provides\": [\"screens\"], \"consumes\": [\"provider\"]},"
    " \"services\": {\"provides\": [\"greeter:hello\", {\"id\": \"greeter:fast\", \"attributes\": {\"latency\": 5}}],"
    "   \"consumes\": [\"clock\", {\"id\": \"store\", \"optional\": false, \"provider\": \"local:store\","
    "                  \"constraints\": {\"latency\": 10}}]},"
    " \"permissions\": [\"network\"], \"ui\": {\"unknown\": [1, 2, 3]}}";

static const char *const DEEP_WRONG_TYPE =
    "{\"importName\": \"greeter\", \"targets\": [\"app.one\"],"
    " \"services\": {\"provides\": [\"a\"], \"consumes\": [\"b\", {\"id\": \"c\", \"constraints\": {\"x\": \"fast\"}}]}}";

static void release_frees_everything(void) {
    weivel_standard_manifest *manifest = NULL;
    CHECK(weivel_standard_memory_live_blocks() == 0);
    CHECK(weivel_standard_read_manifest(FULL_MANIFEST, strlen(FULL_MANIFEST), &manifest, NULL) == WEIVEL_STATUS_OK);
    CHECK(weivel_standard_memory_live_blocks() > 0);
    weivel_standard_manifest_release(manifest);
    CHECK(weivel_standard_memory_live_blocks() == 0);
}

static void a_refusal_release_frees_everything(void) {
    weivel_standard_manifest *manifest = NULL;
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    CHECK(weivel_standard_read_manifest(DEEP_WRONG_TYPE, strlen(DEEP_WRONG_TYPE), &manifest, &refusal) ==
          WEIVEL_STATUS_REFUSED);
    CHECK(weivel_standard_memory_live_blocks() > 0);
    weivel_standard_refusal_release(&refusal);
    CHECK(weivel_standard_memory_live_blocks() == 0);
}

/** Fails each allocation in turn until a read needs no more, and checks every failure leaves nothing behind. */
static void every_failed_allocation_leaves_nothing(const char *text, weivel_status settled) {
    uint64_t nth;
    for (nth = 1; nth < 100000; nth++) {
        weivel_standard_manifest *manifest = NULL;
        weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
        weivel_status status;
        int failed;
        weivel_standard_memory_fail_at(nth);
        status = weivel_standard_read_manifest(text, strlen(text), &manifest, &refusal);
        failed = weivel_standard_memory_failed();
        weivel_standard_memory_fail_at(0);
        if (!failed) {
            CHECK(status == settled);
            weivel_standard_manifest_release(manifest);
            weivel_standard_refusal_release(&refusal);
            CHECK(weivel_standard_memory_live_blocks() == 0);
            CHECK(nth > 1);
            return;
        }
        if (status != WEIVEL_STATUS_OUT_OF_MEMORY) {
            fprintf(stderr, "failing allocation %llu answered %d\n", (unsigned long long)nth, (int)status);
        }
        CHECK(status == WEIVEL_STATUS_OUT_OF_MEMORY);
        CHECK(manifest == NULL);
        CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_NONE && refusal.storage == NULL);
        CHECK(weivel_standard_memory_live_blocks() == 0);
    }
    CHECK(!"the read never settled");
}

int main(void) {
    release_frees_everything();
    a_refusal_release_frees_everything();
    every_failed_allocation_leaves_nothing(FULL_MANIFEST, WEIVEL_STATUS_OK);
    every_failed_allocation_leaves_nothing(DEEP_WRONG_TYPE, WEIVEL_STATUS_REFUSED);
    return check_summary("manifest_memory_test");
}
