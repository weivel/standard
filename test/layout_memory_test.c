#include "allocation.h"
#include "check.h"
#include "fixtures.h"
#include "weivel/layout.h"

static const char *const ROOT = WEIVEL_STANDARD_FIXTURE_ROOT;

static int lay_out(char *directory) {
    int written = 1;
    fixture_make_directory(ROOT);
    fixture_join(directory, ROOT, "memory");
    written &= fixture_fresh_directory(directory);
    written &= fixture_write(directory, "kept.json", "{\"importName\": \"kept\", \"api\": 2, \"entry\": \"kept.bin\"}");
    written &= fixture_write(directory, "kept_identity.json", "{\"id\": \"forge:4/a/b\", \"name\": \"o/kept\"}");
    written &= fixture_write(directory, "kept.bin", "entry");
    written &= fixture_write(directory, "local.json", "{\"importName\": \"local\", \"api\": 2}");
    written &= fixture_write(directory, "twin.json", "{\"importName\": \"twin\", \"api\": 2}");
    written &= fixture_write(directory, "twin_identity.json", "{\"id\": \"forge:4/a/b\", \"name\": \"o/twin\"}");
    written &= fixture_write(directory, "invalid.json", "{\"importName\": \"invalid\", \"api\": -1}");
    written &= fixture_write(directory, "other.json", "{\"importName\": \"another\", \"api\": 2}");
    written &= fixture_write(directory, "bad.json", "{\"importName\": \"bad\", \"api\": 2}");
    written &= fixture_write(directory, "bad_identity.json", "{\"id\": \"x\", \"name\": \"o/bad\"}");
    written &= fixture_write(directory, "notes.txt", "stray");
    return written;
}

/** Fails each allocation in turn until a scan needs no more, and checks every failure leaves nothing behind. */
static void every_failed_allocation_leaves_nothing(const char *directory) {
    uint64_t nth;
    for (nth = 1; nth < 100000; nth++) {
        weivel_standard_discovery discovery = WEIVEL_INIT(weivel_standard_discovery);
        weivel_status status;
        weivel_standard_memory_fail_at(nth);
        status = weivel_standard_discover(directory, NULL, &discovery);
        if (!weivel_standard_memory_failed()) {
            weivel_standard_memory_fail_at(0);
            CHECK(status == WEIVEL_STATUS_OK && discovery.plugin_count == 2 && discovery.failure_count == 4);
            weivel_standard_discovery_release(&discovery);
            CHECK(weivel_standard_memory_live_blocks() == 0);
            return;
        }
        weivel_standard_memory_fail_at(0);
        CHECK(status == WEIVEL_STATUS_OUT_OF_MEMORY);
        CHECK(discovery.plugins == NULL && discovery.failures == NULL && discovery.refusal.storage == NULL);
        CHECK(weivel_standard_memory_live_blocks() == 0);
    }
    CHECK(0);
}

static void a_missing_directory_leaves_nothing(void) {
    char directory[FIXTURE_PATH_CAPACITY];
    uint64_t nth;
    fixture_join(directory, ROOT, "memory-missing");
    fixture_remove_directory(directory);
    for (nth = 1; nth < 1000; nth++) {
        weivel_standard_discovery discovery = WEIVEL_INIT(weivel_standard_discovery);
        weivel_status status;
        weivel_standard_memory_fail_at(nth);
        status = weivel_standard_discover(directory, NULL, &discovery);
        if (!weivel_standard_memory_failed()) {
            weivel_standard_memory_fail_at(0);
            CHECK(status == WEIVEL_STATUS_REFUSED);
            weivel_standard_discovery_release(&discovery);
            CHECK(weivel_standard_memory_live_blocks() == 0);
            return;
        }
        weivel_standard_memory_fail_at(0);
        CHECK(status == WEIVEL_STATUS_OUT_OF_MEMORY && discovery.refusal.storage == NULL);
        CHECK(weivel_standard_memory_live_blocks() == 0);
    }
    CHECK(0);
}

int main(void) {
    char directory[FIXTURE_PATH_CAPACITY];
    CHECK(lay_out(directory));
    every_failed_allocation_leaves_nothing(directory);
    a_missing_directory_leaves_nothing();
    return check_summary("layout_memory_test");
}
