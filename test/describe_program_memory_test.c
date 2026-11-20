#include <stdio.h>
#include <string.h>

#include "allocation.h"
#include "check.h"
#include "files.h"
#include "platform.h"
#include "program.h"

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

static const char STALE[] = "left by an earlier run\n";

static int output_exists(const char *path) {
    weivel_standard_file_kind kind = WEIVEL_STANDARD_FILE_NOTHING;
    CHECK(weivel_standard_files_kind(path, &kind) == WEIVEL_STANDARD_FILES_DONE);
    return kind != WEIVEL_STANDARD_FILE_NOTHING;
}

/**
 * Fails each allocation of one whole run in turn, an earlier run's source waiting at `--output` every time, until a
 * run allocates nothing that fails: every failed run exits FAILED and leaves no source behind, and the last one
 * writes it.
 */
static void exits_failed_without_a_stale_source_wherever_memory_runs_out(void) {
    char *arguments[] = {"weivel-describe", "--definition", "weivel_standard_paths", "--symbol", "paths_described",
                         "--prefix", "paths_", "--output", WEIVEL_DESCRIBE_PROGRAM_OUTPUT,
                         WEIVEL_DESCRIBE_SERVICES_HEADER};
    uint64_t nth;
    for (nth = 1;; nth++) {
        int code;
        int failed;
        CHECK(describe_write_file(WEIVEL_DESCRIBE_PROGRAM_OUTPUT, STALE, sizeof STALE - 1));
        weivel_standard_memory_fail_at(nth);
        code = describe_program(arguments, COUNT_OF(arguments));
        failed = weivel_standard_memory_failed();
        weivel_standard_memory_fail_at(0);
        if (failed) {
            if (code != DESCRIBE_EXIT_FAILED || output_exists(WEIVEL_DESCRIBE_PROGRAM_OUTPUT)) {
                fprintf(stderr, "failing allocation %u: exit %d, the stale source %s\n", (unsigned)nth, code,
                        output_exists(WEIVEL_DESCRIBE_PROGRAM_OUTPUT) ? "kept" : "removed");
                check_failures++;
            }
        } else {
            CHECK(code == DESCRIBE_EXIT_DESCRIBED);
            CHECK(output_exists(WEIVEL_DESCRIBE_PROGRAM_OUTPUT));
        }
        if (weivel_standard_memory_live_blocks() != 0) {
            fprintf(stderr, "failing allocation %u: %d blocks live\n", (unsigned)nth,
                    (int)weivel_standard_memory_live_blocks());
            check_failures++;
        }
        if (!failed) {
            break;
        }
    }
    CHECK(nth > 1);
}

int main(void) {
    exits_failed_without_a_stale_source_wherever_memory_runs_out();
    return check_summary("describe_program_memory_test");
}
