#include "allocation.h"
#include "check.h"
#include "weivel/library.h"

static const char *const SAMPLES[] = {
    "{\"importName\": \"echo\", \"version\": \"1\", \"release\": \"echo_release\", \"services\": ["
    "{\"id\": \"echo\", \"functions\": [{\"name\": \"repeat\", \"symbol\": \"echo_repeat\", \"kind\": \"json\","
    " \"argument\": {\"type\": \"string\"}, \"result\": true, \"context\": [\"home\"]}]},"
    "{\"id\": \"other\", \"functions\": [{\"name\": \"a\", \"symbol\": \"a\", \"kind\": \"json\", \"argument\": true,"
    " \"result\": true}, {\"name\": \"b\", \"symbol\": \"b\", \"kind\": \"json\", \"argument\": true,"
    " \"result\": true}]}]}",
    "{\"importName\": \"echo\", \"version\": \"1\", \"release\": \"echo_release\", \"services\": ["
    "{\"id\": \"echo\", \"functions\": [{\"name\": \"echo\", \"symbol\": \"echo_echo\", \"kind\": \"bytes\"}]}]}",
    "{\"importName\": \"echo\", \"version\": \"1\", \"release\": \"r\", \"services\": ["
    "{\"id\": \"echo\", \"functions\": [{\"name\": \"echo\", \"symbol\": \"echo_echo\", \"kind\": \"bytes\","
    " \"result\": true}]}]}",
    "{\"importName\": \"echo\", \"version\": \"1\", \"release\": \"r\", \"services\": [{\"id\": \"Echo\"}]}",
    "{\"importName\": "};

/** Fails each allocation in turn until a read needs no more, and checks every failure leaves nothing behind. */
static void every_failed_allocation_leaves_nothing(const char *text) {
    uint64_t nth;
    for (nth = 1; nth < 10000; nth++) {
        weivel_standard_library library = WEIVEL_INIT(weivel_standard_library);
        weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
        weivel_status status;
        weivel_standard_memory_fail_at(nth);
        status = weivel_standard_read_library(text, strlen(text), &library, &refusal);
        if (!weivel_standard_memory_failed()) {
            weivel_standard_memory_fail_at(0);
            weivel_standard_library_release(&library);
            weivel_standard_refusal_release(&refusal);
            CHECK(weivel_standard_memory_live_blocks() == 0);
            return;
        }
        weivel_standard_memory_fail_at(0);
        CHECK(status == WEIVEL_STATUS_OUT_OF_MEMORY);
        CHECK(library.storage == NULL && library.services == NULL);
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
    return check_summary("library_memory_test");
}
