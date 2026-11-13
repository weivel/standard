#include "check.h"
#include "patterns.h"
#include "repository_id_samples.h"
#include "weivel/identity.h"

#include <stdint.h>

static weivel_status parse_text(const char *text, weivel_standard_identity *identity,
                                weivel_standard_refusal *refusal) {
    return weivel_standard_identity_parse(text, strlen(text), identity, refusal);
}

static void check_forge_parts(const char *text, const char *forge, const char *host, uint64_t number,
                              size_t segment_count, const char *last_segment) {
    weivel_standard_identity identity = WEIVEL_INIT(weivel_standard_identity);
    CHECK(parse_text(text, &identity, NULL) == WEIVEL_STATUS_OK);
    CHECK(identity.kind == WEIVEL_STANDARD_IDENTITY_FORGE);
    check_text(identity.forge, forge, text);
    CHECK((identity.host == NULL) == (host == NULL));
    if (host != NULL && identity.host != NULL) {
        check_text(identity.host, host, text);
    }
    CHECK(identity.number == number);
    CHECK(identity.segment_count == segment_count);
    if (last_segment != NULL && identity.segment_count == segment_count && segment_count > 0) {
        check_text(identity.segments[segment_count - 1], last_segment, text);
    }
    CHECK(identity.name == NULL);
    weivel_standard_identity_release(&identity);
}

static void parts_of_forge_identities(void) {
    check_forge_parts("forge:1434954468", "forge", NULL, 1434954468u, 0, NULL);
    check_forge_parts("gitea@git.example.com:123", "gitea", "git.example.com", 123, 0, NULL);
    check_forge_parts("forge:1361969293/kernel", "forge", NULL, 1361969293u, 1, "kernel");
    check_forge_parts("a-1:0/x_y.z/.hidden/a..b", "a-1", NULL, 0, 3, "a..b");
    check_forge_parts("forge:00042", "forge", NULL, 42, 0, NULL);
    check_forge_parts("forge:18446744073709551615", "forge", NULL, UINT64_MAX, 0, NULL);
}

static void parts_of_local_identities(void) {
    weivel_standard_identity identity = WEIVEL_INIT(weivel_standard_identity);
    CHECK(parse_text("local:plugin-stub", &identity, NULL) == WEIVEL_STATUS_OK);
    CHECK(identity.kind == WEIVEL_STANDARD_IDENTITY_LOCAL);
    check_text(identity.name, "plugin-stub", "local name");
    CHECK(identity.name_length == strlen("plugin-stub"));
    CHECK(identity.forge == NULL && identity.host == NULL && identity.segment_count == 0);
    weivel_standard_identity_release(&identity);

    CHECK(parse_text("local:a:b/x/y", &identity, NULL) == WEIVEL_STATUS_OK);
    check_text(identity.name, "a:b", "local name with a colon");
    CHECK(identity.segment_count == 2);
    check_text(identity.segments[0], "x", "first segment");
    check_text(identity.segments[1], "y", "second segment");
    weivel_standard_identity_release(&identity);

    CHECK(weivel_standard_identity_parse("local:a\0b/s", 11, &identity, NULL) == WEIVEL_STATUS_OK);
    CHECK(identity.name_length == 3 && memcmp(identity.name, "a\0b", 4) == 0);
    CHECK(identity.segment_count == 1);
    weivel_standard_identity_release(&identity);

    CHECK(parse_text("local:12", &identity, NULL) == WEIVEL_STATUS_OK);
    CHECK(identity.kind == WEIVEL_STANDARD_IDENTITY_LOCAL);
    weivel_standard_identity_release(&identity);
}

static int order_of_texts(const char *left_text, const char *right_text) {
    weivel_standard_identity left = WEIVEL_INIT(weivel_standard_identity);
    weivel_standard_identity right = WEIVEL_INIT(weivel_standard_identity);
    int order;
    CHECK(parse_text(left_text, &left, NULL) == WEIVEL_STATUS_OK);
    CHECK(parse_text(right_text, &right, NULL) == WEIVEL_STATUS_OK);
    order = weivel_standard_identity_compare(&left, &right);
    CHECK(weivel_standard_identity_compare(&right, &left) == -order);
    CHECK(weivel_standard_identity_compare(&left, &left) == 0);
    weivel_standard_identity_release(&left);
    weivel_standard_identity_release(&right);
    return order;
}

static void compare_orders_by_parts(void) {
    CHECK(order_of_texts("forge:9", "forge:10") < 0);
    CHECK(order_of_texts("forge:10", "forge:9") > 0);
    CHECK(order_of_texts("forge:007", "forge:7") == 0);
    CHECK(order_of_texts("forge:1", "forge:1") == 0);
    CHECK(order_of_texts("local:z", "forge:1") < 0);
    CHECK(order_of_texts("a:5", "b:1") < 0);
    CHECK(order_of_texts("forge:5", "forge@host:5") < 0);
    CHECK(order_of_texts("forge@a.com:5", "forge@b.com:1") < 0);
    CHECK(order_of_texts("forge@host:5", "forge@host:6") < 0);
    CHECK(order_of_texts("forge:1", "forge:1/a") < 0);
    CHECK(order_of_texts("forge:1/a", "forge:1/b") < 0);
    CHECK(order_of_texts("forge:1/a/z", "forge:1/b") < 0);
    CHECK(order_of_texts("forge:1/a", "forge:1/a/b") < 0);
    CHECK(order_of_texts("forge:1/a/b", "forge:1/a/b") == 0);
    CHECK(order_of_texts("local:ab", "local:b") < 0);
    CHECK(order_of_texts("local:a", "local:ab") < 0);
    CHECK(order_of_texts("local:a/x", "local:a/y") < 0);
    CHECK(order_of_texts("forge:18446744073709551615", "forge:18446744073709551614") > 0);
}

static void refusal_of(const char *text, uint32_t code, const char *detail_part) {
    weivel_standard_identity identity = WEIVEL_INIT(weivel_standard_identity);
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    CHECK(parse_text(text, &identity, &refusal) == WEIVEL_STATUS_REFUSED);
    CHECK(refusal.code == code);
    CHECK(refusal.path != NULL && refusal.path[0] == '\0');
    CHECK(refusal.fix != NULL && refusal.fix[0] != '\0');
    CHECK(refusal.detail != NULL && strstr(refusal.detail, detail_part) != NULL);
    if (refusal.detail == NULL || strstr(refusal.detail, detail_part) == NULL) {
        fprintf(stderr, "  \"%s\" gave detail \"%s\"\n", text, refusal.detail == NULL ? "(null)" : refusal.detail);
    }
    CHECK(identity.kind == 0 && identity.name == NULL && identity.forge == NULL && identity.segments == NULL);
    weivel_standard_refusal_release(&refusal);
}

static void a_refusal_says_where_it_failed(void) {
    refusal_of("calendar", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "byte 8:");
    refusal_of("", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "byte 0:");
    refusal_of("Forge:1", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "byte 0:");
    refusal_of("forge:", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "byte 6:");
    refusal_of("forge:abc", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "byte 6:");
    refusal_of("forge@:1", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "byte 6:");
    refusal_of("forge:1/", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "byte 8:");
    refusal_of("forge:1/..", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "byte 8:");
    refusal_of("forge:1/a/../b", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "byte 10:");
    refusal_of("forge:1/X", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "byte 8:");
    refusal_of("local:", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "byte 6:");
    refusal_of("local:a b", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "byte 7:");
    refusal_of("local:a\xc2\xa0", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "byte 7:");
    refusal_of("local:ab/.", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "byte 9:");
}

static void a_number_past_64_bits_is_refused_on_its_own(void) {
    refusal_of("forge:18446744073709551616", WEIVEL_STANDARD_REFUSAL_UNREPRESENTABLE, "does not fit 64 bits");
    refusal_of("forge@host:99999999999999999999999/x", WEIVEL_STANDARD_REFUSAL_UNREPRESENTABLE,
               "does not fit 64 bits");
    refusal_of("forge:184467440737095516150", WEIVEL_STANDARD_REFUSAL_UNREPRESENTABLE, "byte 6:");
}

static void malformed_calls_and_missing_refusal(void) {
    weivel_standard_identity identity = WEIVEL_INIT(weivel_standard_identity);
    CHECK(weivel_standard_identity_parse(NULL, 0, &identity, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(weivel_standard_identity_parse("forge:1", 7, NULL, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(parse_text("nope", &identity, NULL) == WEIVEL_STATUS_REFUSED);
    weivel_standard_identity_release(NULL);
}

static void check_parse_agrees_with_matcher(const char *label, const char *text, size_t length, int expected_valid) {
    weivel_standard_identity identity = WEIVEL_INIT(weivel_standard_identity);
    weivel_status status = weivel_standard_identity_parse(text, length, &identity, NULL);
    int matches = weivel_standard_matches_repository_id(text, length);
    if (matches != expected_valid) {
        fprintf(stderr, "%s: the matcher answered %d, expected %d\n", label, matches, expected_valid);
        check_failures++;
    }
    if ((status == WEIVEL_STATUS_OK) != matches) {
        fprintf(stderr, "%s: parse answered %d but the matcher answered %d\n", label, (int)status, matches);
        check_failures++;
    }
    if (status == WEIVEL_STATUS_OK) {
        weivel_standard_identity_release(&identity);
    }
}

typedef struct identity_case {
    const char *value;
    int valid;
} identity_case;

static const identity_case IDENTITY_CASES[] = {
    {"forge:1434954468", 1},
    {"gitea@git.example.com:123", 1},
    {"local:plugin-stub", 1},
    {"forge:1361969293/kernel", 1},
    {"forge:1/a..b", 1},
    {"forge:1/.hidden", 1},
    {"calendar", 0},
    {"forge:1/../calendar_identity", 0},
    {"forge:1/a/../b", 0},
    {"forge:1/..", 0},
    {"forge:abc", 0},
    {"forge:1/.", 0},
    {"forge:", 0},
    {"Forge:1", 0},
};

static void parse_agrees_with_the_identity_cases(void) {
    size_t at;
    for (at = 0; at < sizeof IDENTITY_CASES / sizeof IDENTITY_CASES[0]; at++) {
        check_parse_agrees_with_matcher(IDENTITY_CASES[at].value, IDENTITY_CASES[at].value,
                                        strlen(IDENTITY_CASES[at].value), IDENTITY_CASES[at].valid);
    }
}

static void parse_agrees_with_every_pattern_sample(void) {
    size_t at;
    for (at = 0; at < sizeof REPOSITORY_IDS / sizeof REPOSITORY_IDS[0]; at++) {
        weivel_standard_identity identity = WEIVEL_INIT(weivel_standard_identity);
        weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
        weivel_status status =
            weivel_standard_identity_parse(REPOSITORY_IDS[at].text, REPOSITORY_IDS[at].length, &identity, &refusal);
        if (REPOSITORY_IDS[at].matches) {
            CHECK(status == WEIVEL_STATUS_OK);
            weivel_standard_identity_release(&identity);
        } else {
            CHECK(status == WEIVEL_STATUS_REFUSED);
            CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_INVALID_VALUE);
        }
        weivel_standard_refusal_release(&refusal);
    }
}

int main(void) {
    parts_of_forge_identities();
    parts_of_local_identities();
    compare_orders_by_parts();
    a_refusal_says_where_it_failed();
    a_number_past_64_bits_is_refused_on_its_own();
    malformed_calls_and_missing_refusal();
    parse_agrees_with_the_identity_cases();
    parse_agrees_with_every_pattern_sample();
    return check_summary("identity_test");
}
