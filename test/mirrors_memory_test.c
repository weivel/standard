#include "allocation.h"
#include "check.h"
#include "weivel/mirrors.h"
#include "weivel/sha256.h"

#define HASH "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"

static const char *const SAMPLES[] = {
    "{\"mirrors\": [\"https://example.org/core/{sha256}\", \"file:///srv/core/{sha256}\"]}",
    "{\"mirrors\": [\"https://example.org/core\"]}", "{\"mirrors\": ["};

/** Fails each allocation in turn until a read needs no more, and checks every failure leaves nothing behind. */
static void every_failed_read_leaves_nothing(const char *text) {
    uint64_t nth;
    for (nth = 1; nth < 10000; nth++) {
        weivel_standard_mirror_list list = WEIVEL_INIT(weivel_standard_mirror_list);
        weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
        weivel_status status;
        weivel_standard_memory_fail_at(nth);
        status = weivel_standard_read_mirrors(text, strlen(text), &list, &refusal);
        if (!weivel_standard_memory_failed()) {
            weivel_standard_memory_fail_at(0);
            weivel_standard_mirror_list_release(&list);
            weivel_standard_refusal_release(&refusal);
            CHECK(weivel_standard_memory_live_blocks() == 0);
            return;
        }
        weivel_standard_memory_fail_at(0);
        CHECK(status == WEIVEL_STATUS_OUT_OF_MEMORY);
        CHECK(list.storage == NULL && list.templates == NULL);
        weivel_standard_refusal_release(&refusal);
        CHECK(weivel_standard_memory_live_blocks() == 0);
    }
    CHECK(0);
}

static void every_failed_expansion_leaves_nothing(void) {
    uint64_t nth;
    for (nth = 1; nth < 100; nth++) {
        char *url = NULL;
        weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
        weivel_status status;
        weivel_standard_memory_fail_at(nth);
        status = weivel_standard_mirror_expand("https://example.org/{sha256}", HASH, &url, &refusal);
        if (!weivel_standard_memory_failed()) {
            weivel_standard_memory_fail_at(0);
            weivel_standard_text_release(url);
            weivel_standard_refusal_release(&refusal);
            CHECK(weivel_standard_memory_live_blocks() == 0);
            return;
        }
        weivel_standard_memory_fail_at(0);
        CHECK(status == WEIVEL_STATUS_OUT_OF_MEMORY && url == NULL);
        weivel_standard_refusal_release(&refusal);
        CHECK(weivel_standard_memory_live_blocks() == 0);
    }
    CHECK(0);
}

static void every_failed_mismatch_refusal_leaves_nothing(void) {
    uint64_t nth;
    for (nth = 1; nth < 100; nth++) {
        weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
        weivel_status status;
        weivel_standard_memory_fail_at(nth);
        status = weivel_standard_verify_download("abd", 3, HASH, &refusal);
        if (!weivel_standard_memory_failed()) {
            weivel_standard_memory_fail_at(0);
            weivel_standard_refusal_release(&refusal);
            CHECK(weivel_standard_memory_live_blocks() == 0);
            return;
        }
        weivel_standard_memory_fail_at(0);
        CHECK(status == WEIVEL_STATUS_OUT_OF_MEMORY);
        weivel_standard_refusal_release(&refusal);
        CHECK(weivel_standard_memory_live_blocks() == 0);
    }
    CHECK(0);
}

int main(void) {
    size_t at;
    for (at = 0; at < sizeof SAMPLES / sizeof SAMPLES[0]; at++) {
        every_failed_read_leaves_nothing(SAMPLES[at]);
    }
    every_failed_expansion_leaves_nothing();
    every_failed_mismatch_refusal_leaves_nothing();
    return check_summary("mirrors_memory_test");
}
