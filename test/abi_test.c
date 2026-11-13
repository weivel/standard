#include "check.h"
#include "weivel/identity.h"
#include "weivel/json.h"
#include "weivel/library.h"
#include "weivel/mirrors.h"
#include "weivel/sha256.h"
#include "weivel/standard.h"

#include <stddef.h>

#define GUARD_BYTE 0xab
#define NEWER_BYTE 0xff

static const char BARE_SERVICE[] = "{\"importName\": \"a\", \"api\": 2, \"services\": {\"provides\": [\"calendar\"]}}";
static const char MALFORMED_JSON[] = "{\"importName\": ";

typedef struct newer_options {
    weivel_standard_validation_options options;
    uint32_t appended_later;
} newer_options;

typedef struct guarded_refusal {
    weivel_standard_refusal refusal;
    unsigned char guard[32];
} guarded_refusal;

typedef struct newer_refusal {
    weivel_standard_refusal refusal;
    uint64_t appended_later[4];
} newer_refusal;

typedef struct newer_validation {
    weivel_standard_validation validation;
    uint64_t appended_later[4];
} newer_validation;

typedef struct guarded_error {
    weivel_json_error error;
    unsigned char guard[32];
} guarded_error;

typedef struct newer_error {
    weivel_json_error error;
    uint64_t appended_later[4];
} newer_error;

static weivel_status validate_bare_service(const weivel_standard_validation_options *options) {
    weivel_standard_validation validation = WEIVEL_INIT(weivel_standard_validation);
    weivel_status status = weivel_standard_validate(BARE_SERVICE, strlen(BARE_SERVICE), options, &validation);
    weivel_standard_validation_release(&validation);
    return status;
}

static int all_bytes_are(const unsigned char *bytes, size_t count, unsigned char wanted) {
    size_t at;
    for (at = 0; at < count; at++) {
        if (bytes[at] != wanted) {
            return 0;
        }
    }
    return 1;
}

static void options_of_the_previous_size_take_defaults_for_what_they_lack(void) {
    weivel_strings none = {NULL, 0};
    weivel_standard_validation_options options = WEIVEL_INIT(weivel_standard_validation_options);
    options.well_known_services = &none;
    CHECK(validate_bare_service(&options) == WEIVEL_STATUS_REFUSED);

    options.size = (uint32_t)offsetof(weivel_standard_validation_options, well_known_services);
    CHECK(validate_bare_service(&options) == WEIVEL_STATUS_OK);
}

static void options_from_a_newer_caller_are_read_up_to_what_the_standard_knows(void) {
    weivel_strings none = {NULL, 0};
    newer_options newer = {WEIVEL_INIT(weivel_standard_validation_options), 0xffffffffu};
    newer.options.size = (uint32_t)sizeof newer;
    newer.options.well_known_services = &none;
    CHECK(validate_bare_service(&newer.options) == WEIVEL_STATUS_REFUSED);
}

static void a_refusal_is_written_only_within_the_callers_size(void) {
    weivel_standard_manifest *manifest = NULL;
    guarded_refusal guarded;
    memset(&guarded, GUARD_BYTE, sizeof guarded);
    guarded.refusal.size = (uint32_t)sizeof guarded.refusal;
    guarded.refusal.code = WEIVEL_STANDARD_REFUSAL_NONE;
    guarded.refusal.storage = NULL;
    CHECK(weivel_standard_read_manifest(MALFORMED_JSON, strlen(MALFORMED_JSON), &manifest, &guarded.refusal) ==
          WEIVEL_STATUS_REFUSED);
    CHECK(guarded.refusal.code == WEIVEL_STANDARD_REFUSAL_MALFORMED_JSON);
    CHECK(guarded.refusal.size == sizeof guarded.refusal);
    weivel_standard_refusal_release(&guarded.refusal);
    CHECK(guarded.refusal.size == sizeof guarded.refusal && guarded.refusal.storage == NULL);
    CHECK(all_bytes_are(guarded.guard, sizeof guarded.guard, GUARD_BYTE));
}

static void a_newer_callers_appended_refusal_fields_read_as_zero_and_keep_its_size(void) {
    weivel_standard_manifest *manifest = NULL;
    newer_refusal newer;
    memset(&newer, NEWER_BYTE, sizeof newer);
    newer.refusal.size = (uint32_t)sizeof newer;
    newer.refusal.code = WEIVEL_STANDARD_REFUSAL_NONE;
    newer.refusal.storage = NULL;
    CHECK(weivel_standard_read_manifest(MALFORMED_JSON, strlen(MALFORMED_JSON), &manifest, &newer.refusal) ==
          WEIVEL_STATUS_REFUSED);
    CHECK(newer.refusal.size == sizeof newer);
    CHECK(all_bytes_are((const unsigned char *)newer.appended_later, sizeof newer.appended_later, 0));
    weivel_standard_refusal_release(&newer.refusal);
}

static void a_refusal_smaller_than_its_first_shape_is_malformed_and_untouched(void) {
    weivel_standard_manifest *manifest = NULL;
    weivel_standard_refusal small = WEIVEL_INIT(weivel_standard_refusal);
    small.size = (uint32_t)offsetof(weivel_standard_refusal, storage);
    CHECK(weivel_standard_read_manifest(MALFORMED_JSON, strlen(MALFORMED_JSON), &manifest, &small) ==
          WEIVEL_STATUS_MALFORMED);
    CHECK(small.code == WEIVEL_STANDARD_REFUSAL_NONE && small.detail == NULL && small.storage == NULL);
    CHECK(weivel_standard_verify_download(NULL, 0, "", &small) == WEIVEL_STATUS_MALFORMED);
    CHECK(small.code == WEIVEL_STANDARD_REFUSAL_NONE && small.storage == NULL);
}

static void an_answer_smaller_than_its_first_shape_is_malformed(void) {
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    weivel_standard_identity identity = WEIVEL_INIT(weivel_standard_identity);
    weivel_standard_library library = WEIVEL_INIT(weivel_standard_library);
    weivel_standard_mirror_list list = WEIVEL_INIT(weivel_standard_mirror_list);
    identity.size = (uint32_t)offsetof(weivel_standard_identity, segment_count);
    CHECK(weivel_standard_identity_parse("local:a", 7, &identity, &refusal) == WEIVEL_STATUS_MALFORMED);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_MALFORMED_CALL && identity.name == NULL);
    weivel_standard_refusal_release(&refusal);
    library.size = (uint32_t)offsetof(weivel_standard_library, storage);
    CHECK(weivel_standard_read_library("{}", 2, &library, &refusal) == WEIVEL_STATUS_MALFORMED);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_MALFORMED_CALL && library.import_name == NULL);
    weivel_standard_refusal_release(&refusal);
    list.size = (uint32_t)offsetof(weivel_standard_mirror_list, storage);
    CHECK(weivel_standard_read_mirrors("{}", 2, &list, &refusal) == WEIVEL_STATUS_MALFORMED);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_MALFORMED_CALL && list.templates == NULL);
    weivel_standard_refusal_release(&refusal);
}

static void a_refusal_handed_out_inside_a_validation_reports_its_size(void) {
    newer_validation newer;
    memset(&newer, NEWER_BYTE, sizeof newer);
    memset(&newer.validation, 0, sizeof newer.validation);
    newer.validation.size = (uint32_t)sizeof newer;
    CHECK(weivel_standard_validate(MALFORMED_JSON, strlen(MALFORMED_JSON), NULL, &newer.validation) ==
          WEIVEL_STATUS_REFUSED);
    CHECK(newer.validation.size == sizeof newer);
    CHECK(newer.validation.refusal.size == sizeof newer.validation.refusal);
    CHECK(newer.validation.refusal.code == WEIVEL_STANDARD_REFUSAL_MALFORMED_JSON);
    CHECK(all_bytes_are((const unsigned char *)newer.appended_later, sizeof newer.appended_later, 0));
    weivel_standard_validation_release(&newer.validation);
    CHECK(newer.validation.size == sizeof newer);
}

static void identities_and_declarations_keep_the_callers_size(void) {
    weivel_standard_identity identity = WEIVEL_INIT(weivel_standard_identity);
    weivel_standard_library library = WEIVEL_INIT(weivel_standard_library);
    const char *declaration = "{\"importName\": \"echo\", \"version\": \"0.1.0\", \"release\": \"echo_release\", "
                              "\"services\": [{\"id\": \"echo\", \"functions\": [{\"name\": \"repeat\", "
                              "\"symbol\": \"echo_repeat\", \"kind\": \"json\", \"argument\": true, "
                              "\"result\": true}]}]}";
    CHECK(weivel_standard_identity_parse("local:a", 7, &identity, NULL) == WEIVEL_STATUS_OK);
    CHECK(identity.size == sizeof identity && identity.kind == WEIVEL_STANDARD_IDENTITY_LOCAL);
    weivel_standard_identity_release(&identity);
    CHECK(identity.size == sizeof identity && identity.name == NULL);
    CHECK(weivel_standard_read_library(declaration, strlen(declaration), &library, NULL) == WEIVEL_STATUS_OK);
    CHECK(library.size == sizeof library && library.service_count == 1);
    weivel_standard_library_release(&library);
    CHECK(library.size == sizeof library && library.storage == NULL);
}

static void a_json_error_is_filled_only_within_its_size(void) {
    guarded_error guarded;
    newer_error newer;
    weivel_json_error small = {.size = (uint32_t)offsetof(weivel_json_error, out_of_memory)};
    memset(&guarded, GUARD_BYTE, sizeof guarded);
    guarded.error.size = (uint32_t)sizeof guarded.error;
    CHECK(weivel_json_parse(MALFORMED_JSON, strlen(MALFORMED_JSON), &guarded.error) == NULL);
    CHECK(guarded.error.message != NULL && guarded.error.line == 1);
    CHECK(all_bytes_are(guarded.guard, sizeof guarded.guard, GUARD_BYTE));

    memset(&newer, NEWER_BYTE, sizeof newer);
    newer.error.size = (uint32_t)sizeof newer;
    CHECK(weivel_json_parse(MALFORMED_JSON, strlen(MALFORMED_JSON), &newer.error) == NULL);
    CHECK(newer.error.size == sizeof newer && newer.error.message != NULL);
    CHECK(all_bytes_are((const unsigned char *)newer.appended_later, sizeof newer.appended_later, 0));

    CHECK(weivel_json_parse(MALFORMED_JSON, strlen(MALFORMED_JSON), &small) == NULL);
    CHECK(small.message == NULL && small.line == 0);
}

int main(void) {
    options_of_the_previous_size_take_defaults_for_what_they_lack();
    options_from_a_newer_caller_are_read_up_to_what_the_standard_knows();
    a_refusal_is_written_only_within_the_callers_size();
    a_newer_callers_appended_refusal_fields_read_as_zero_and_keep_its_size();
    a_refusal_smaller_than_its_first_shape_is_malformed_and_untouched();
    an_answer_smaller_than_its_first_shape_is_malformed();
    a_refusal_handed_out_inside_a_validation_reports_its_size();
    identities_and_declarations_keep_the_callers_size();
    a_json_error_is_filled_only_within_its_size();
    return check_summary("abi_test");
}
