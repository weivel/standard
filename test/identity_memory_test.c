#include "allocation.h"
#include "check.h"
#include "weivel/identity.h"

static const char *const SAMPLES[] = {"forge@host.example:42/a/b/c", "local:name/x", "forge:1",
                                      "forge:18446744073709551616", "forge:1/..",
                                      "calendar", "local:a b/x"};

/** Fails each allocation in turn until a parse needs no more, and checks every failure leaves nothing behind. */
static void every_failed_allocation_leaves_nothing(const char *text) {
    uint64_t nth;
    for (nth = 1; nth < 1000; nth++) {
        weivel_standard_identity identity = WEIVEL_INIT(weivel_standard_identity);
        weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
        weivel_status status;
        weivel_standard_memory_fail_at(nth);
        status = weivel_standard_identity_parse(text, strlen(text), &identity, &refusal);
        if (!weivel_standard_memory_failed()) {
            weivel_standard_memory_fail_at(0);
            weivel_standard_identity_release(&identity);
            weivel_standard_refusal_release(&refusal);
            CHECK(weivel_standard_memory_live_blocks() == 0);
            return;
        }
        weivel_standard_memory_fail_at(0);
        CHECK(status == WEIVEL_STATUS_OUT_OF_MEMORY);
        CHECK(identity.name == NULL && identity.forge == NULL && identity.host == NULL && identity.segments == NULL);
        weivel_standard_refusal_release(&refusal);
        CHECK(weivel_standard_memory_live_blocks() == 0);
    }
    CHECK(0);
}

int main(void) {
    size_t at;
    for (at = 0; at < sizeof SAMPLES / sizeof SAMPLES[0]; at++) {
        every_failed_allocation_leaves_nothing(SAMPLES[at]);
    }
    return check_summary("identity_memory_test");
}
