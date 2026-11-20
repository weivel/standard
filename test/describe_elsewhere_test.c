#include <string.h>

#include "check.h"
#include "nested.h"

/** Written at build time by weivel_describe with DESCRIBED_ELSEWHERE probe_remote, both headers scanned. */
const weivel_standard_description *elsewhere_lender_description(void);

int main(void) {
    const weivel_standard_description *lender = elsewhere_lender_description();
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    CHECK(weivel_standard_description_check(lender, &refusal) == WEIVEL_STATUS_OK);
    weivel_standard_refusal_release(&refusal);
    CHECK(lender->blocking_count == 1 && strcmp(lender->blocking[0], "lend") == 0);
    CHECK(lender->definition_count == 2);
    if (lender->definition_count == 2) {
        check_text(lender->definitions[0]->definition, "probe_counter", "the counter written beside the lender");
        CHECK(lender->definitions[1] == probe_remote_description());
    }
    CHECK(elsewhere_lender_description() == lender);
    return check_summary("describe_elsewhere_test");
}
