#include "allocation.h"
#include "check.h"
#include "json_locale.h"
#include "weivel/json.h"

static const char *const DOCUMENT =
    "{\"name\": \"boot\", \"escaped\": \"a\\u00e9\\ud83d\\ude00\", \"steps\": [1, 2.5, -3e2, true, null],"
    " \"nested\": {\"empty\": [], \"deep\": {\"key\": \"value\"}}}";

static void bad_text_is_not_out_of_memory(void) {
    static const char *const MALFORMED[] = {"{\"a\": }", "[1, 2", "{\"a\": 1, \"a\": 2}", "1e999", "nul"};
    size_t at;
    for (at = 0; at < sizeof MALFORMED / sizeof MALFORMED[0]; at++) {
        weivel_json_error error = {0};
        weivel_json_value *value = weivel_json_parse(MALFORMED[at], strlen(MALFORMED[at]), &error);
        CHECK(value == NULL);
        CHECK(error.message != NULL);
        CHECK(error.out_of_memory == 0);
        weivel_json_free(value);
    }
}

/** Fails each allocation in turn until the parse needs no more, and checks each failure says memory ran out. */
static void every_failed_allocation_is_out_of_memory(void) {
    uint64_t nth;
    for (nth = 1; nth < 100000; nth++) {
        weivel_json_error error = {0};
        weivel_json_value *value;
        int failed;
        weivel_standard_memory_fail_at(nth);
        value = weivel_json_parse(DOCUMENT, strlen(DOCUMENT), &error);
        failed = weivel_standard_memory_failed();
        weivel_standard_memory_fail_at(0);
        if (!failed) {
            CHECK(value != NULL);
            CHECK(error.out_of_memory == 0);
            weivel_json_free(value);
            CHECK(weivel_standard_memory_live_blocks() == 0);
            CHECK(nth > 1);
            return;
        }
        CHECK(value == NULL);
        if (error.out_of_memory != 1) {
            fprintf(stderr, "failing allocation %llu: out_of_memory %d, message %s\n", (unsigned long long)nth,
                    error.out_of_memory, error.message == NULL ? "(null)" : error.message);
        }
        CHECK(error.out_of_memory == 1);
        check_text(error.message, "out of memory", "message on a failed allocation");
        CHECK(weivel_standard_memory_live_blocks() == 0);
    }
    CHECK(!"the parse never settled");
}

/** One read of the locale both sizes a number's buffer and fills it, so a setlocale between them cannot overrun it. */
static void each_number_reads_the_locale_once(void) {
    static const char NUMBERS[] = "[1.5, 2.25, -3e2, 4]";
    weivel_json_error error = {0};
    int64_t before = weivel_standard_json_locale_reads();
    weivel_json_value *value = weivel_json_parse(NUMBERS, strlen(NUMBERS), &error);
    double real = 0;
    CHECK(value != NULL);
    CHECK(weivel_standard_json_locale_reads() - before == 4);
    CHECK(weivel_json_as_double(weivel_json_array_at(value, 1), &real) && real == 2.25);
    weivel_json_free(value);
}

int main(void) {
    each_number_reads_the_locale_once();
    bad_text_is_not_out_of_memory();
    every_failed_allocation_is_out_of_memory();
    return check_summary("json_memory_test");
}
