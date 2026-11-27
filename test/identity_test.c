#include "check.h"
#include "patterns.h"
#include "repository_id_samples.h"
#include "weivel/identity.h"

#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#define SAME_ROOT "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define LATER_ROOT "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"

static weivel_status parse_text(const char *text, weivel_standard_identity *identity,
                                weivel_standard_refusal *refusal) {
    return weivel_standard_identity_parse(text, strlen(text), identity, refusal);
}

static void check_git_parts(const char *text, const char *commit, size_t segment_count, const char *last_segment) {
    weivel_standard_identity identity = WEIVEL_INIT(weivel_standard_identity);
    CHECK(parse_text(text, &identity, NULL) == WEIVEL_STATUS_OK);
    CHECK(identity.kind == WEIVEL_STANDARD_IDENTITY_GIT);
    check_text(identity.commit, commit, text);
    CHECK(identity.commit_length == strlen(commit));
    CHECK(identity.segment_count == segment_count);
    if (last_segment != NULL && identity.segment_count == segment_count && segment_count > 0) {
        check_text(identity.segments[segment_count - 1], last_segment, text);
    }
    CHECK(identity.name == NULL && identity.name_length == 0);
    weivel_standard_identity_release(&identity);
}

static void parts_of_git_identities(void) {
    check_git_parts("git:" SHA1_ROOT, SHA1_ROOT, 0, NULL);
    check_git_parts("git:" SHA256_ROOT, SHA256_ROOT, 0, NULL);
    check_git_parts("git:" SHA1_ROOT "/kernel", SHA1_ROOT, 1, "kernel");
    check_git_parts("git:" SHA256_ROOT "/x_y.z/.hidden/a..b", SHA256_ROOT, 3, "a..b");
}

static void parts_of_local_identities(void) {
    weivel_standard_identity identity = WEIVEL_INIT(weivel_standard_identity);
    CHECK(parse_text("local:plugin-stub", &identity, NULL) == WEIVEL_STATUS_OK);
    CHECK(identity.kind == WEIVEL_STANDARD_IDENTITY_LOCAL);
    check_text(identity.name, "plugin-stub", "local name");
    CHECK(identity.name_length == strlen("plugin-stub"));
    CHECK(identity.commit == NULL && identity.commit_length == 0 && identity.segment_count == 0);
    weivel_standard_identity_release(&identity);

    CHECK(parse_text("local:a:b/x/y", &identity, NULL) == WEIVEL_STATUS_OK);
    check_text(identity.name, "a:b", "local name with a colon");
    CHECK(identity.segment_count == 2);
    check_text(identity.segments[0], "x", "first segment");
    check_text(identity.segments[1], "y", "second segment");
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
    CHECK(order_of_texts("local:z", "git:" SAME_ROOT) == -1);
    CHECK(order_of_texts("git:" SAME_ROOT, "git:" LATER_ROOT) == -1);
    CHECK(order_of_texts("git:" SAME_ROOT, "git:" SAME_ROOT) == 0);
    CHECK(order_of_texts("git:" SHA1_ROOT, "git:" SHA256_ROOT) == -1);
    CHECK(order_of_texts("git:" SAME_ROOT, "git:" SAME_ROOT "aaaaaaaaaaaaaaaaaaaaaaaa") == -1);
    CHECK(order_of_texts("git:" SAME_ROOT "/z", "git:" LATER_ROOT) == -1);
    CHECK(order_of_texts("git:" SAME_ROOT, "git:" SAME_ROOT "/a") == -1);
    CHECK(order_of_texts("git:" SAME_ROOT "/a", "git:" SAME_ROOT "/b") == -1);
    CHECK(order_of_texts("git:" SAME_ROOT "/a/z", "git:" SAME_ROOT "/b") == -1);
    CHECK(order_of_texts("git:" SAME_ROOT "/a", "git:" SAME_ROOT "/a/b") == -1);
    CHECK(order_of_texts("git:" SAME_ROOT "/a/b", "git:" SAME_ROOT "/a/b") == 0);
    CHECK(order_of_texts("local:ab", "local:b") == -1);
    CHECK(order_of_texts("local:a", "local:ab") == -1);
    CHECK(order_of_texts("local:a/x", "local:a/y") == -1);
}

static size_t offset_in(const char *detail) {
    size_t offset = 0;
    const char *at = detail + strlen("byte ");
    if (strncmp(detail, "byte ", strlen("byte ")) != 0 || *at < '0' || *at > '9') {
        return SIZE_MAX;
    }
    while (*at >= '0' && *at <= '9') {
        offset = offset * 10 + (size_t)(*at - '0');
        at++;
    }
    return *at == ':' ? offset : SIZE_MAX;
}

static void check_refused_at(const char *label, const char *text, size_t length, size_t offset) {
    weivel_standard_identity identity = WEIVEL_INIT(weivel_standard_identity);
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    CHECK(weivel_standard_identity_parse(text, length, &identity, &refusal) == WEIVEL_STATUS_REFUSED);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_INVALID_VALUE);
    CHECK(refusal.path != NULL && refusal.path[0] == '\0');
    CHECK(refusal.fix != NULL && refusal.fix[0] != '\0');
    CHECK(refusal.detail != NULL && offset_in(refusal.detail) == offset);
    if (refusal.detail == NULL || offset_in(refusal.detail) != offset) {
        fprintf(stderr, "  %s gave detail \"%s\", expected byte %zu\n", label,
                refusal.detail == NULL ? "(null)" : refusal.detail, offset);
    }
    CHECK(identity.kind == 0 && identity.name == NULL && identity.commit == NULL && identity.segments == NULL);
    weivel_standard_refusal_release(&refusal);
}

static void refused_at(const char *text, size_t offset) {
    check_refused_at(text, text, strlen(text), offset);
}

static void a_refusal_says_where_it_failed(void) {
    refused_at("", 0);
    refused_at("calendar", 0);
    refused_at("forge:123", 0);
    refused_at("Git:" SHA1_ROOT, 0);
    refused_at("git", 0);
    refused_at("git:", 4);
    refused_at("git:abc", 7);
    refused_at("git:abcx", 7);
    refused_at("git:" SHA1_ROOT "x", 44);
    refused_at("git:" SHA1_ROOT "0", 44);
    refused_at("git:" SHA256_ROOT "0", 68);
    refused_at("git:" SHA1_ROOT "/", 45);
    refused_at("git:" SHA1_ROOT "//a", 45);
    refused_at("git:" SHA1_ROOT "/X", 45);
    refused_at("git:" SHA1_ROOT "/aX", 46);
    refused_at("git:" SHA1_ROOT "/..x/X", 49);
    refused_at("git:" SHA1_ROOT "/a/.", 47);
    refused_at("local:", 6);
    refused_at("local:/a", 6);
    refused_at("local:a b", 7);
    refused_at("local:a\xc2\xa0", 7);
    refused_at("local:ab/.", 9);
    check_refused_at("a NUL in a local name", "local:a\0b/s", 11, 7);
}

static char *long_local_text(size_t name_length, const char *tail) {
    size_t prefix = strlen("local:");
    char *text = malloc(prefix + name_length + strlen(tail) + 1);
    if (text != NULL) {
        memcpy(text, "local:", prefix);
        memset(text + prefix, 'a', name_length);
        memcpy(text + prefix + name_length, tail, strlen(tail) + 1);
    }
    return text;
}

/** Seconds a refusal of `local:` + `name_length` letters + `/A` takes, checking it names the `A`; -1 when it fails. */
static double seconds_to_refuse_a_long_name(size_t name_length) {
    char *text = long_local_text(name_length, "/A");
    clock_t started;
    double seconds;
    CHECK(text != NULL);
    if (text == NULL) {
        return -1;
    }
    started = clock();
    check_refused_at("a long local name", text, strlen(text), strlen("local:") + name_length + 1);
    seconds = (double)(clock() - started) / CLOCKS_PER_SEC;
    free(text);
    return seconds;
}

/**
 * @implNote The small size runs first so a quadratic scan fails in seconds rather than hanging on the large one:
 * 16 KiB took about 3 seconds quadratically, and 1 MiB is what discovery reads from one identity file.
 */
static void a_long_local_name_is_refused_in_linear_time(void) {
    double small = seconds_to_refuse_a_long_name((size_t)16 << 10);
    if (small < 0 || small >= 0.25) {
        fprintf(stderr, "  refusing a 16 KiB local name took %.2f s\n", small);
        check_failures++;
        return;
    }
    {
        double large = seconds_to_refuse_a_long_name((size_t)1 << 20);
        if (large < 0 || large >= 1.0) {
            fprintf(stderr, "  refusing a 1 MiB local name took %.2f s\n", large);
            check_failures++;
        }
    }
}

static void a_forge_number_is_only_an_invalid_value(void) {
    refused_at("forge:18446744073709551616", 0);
    refused_at("forge@host:99999999999999999999999/x", 0);
}

static void malformed_calls_and_missing_refusal(void) {
    weivel_standard_identity identity = WEIVEL_INIT(weivel_standard_identity);
    CHECK(weivel_standard_identity_parse(NULL, 0, &identity, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(weivel_standard_identity_parse("local:a", 7, NULL, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(parse_text("nope", &identity, NULL) == WEIVEL_STATUS_REFUSED);
    weivel_standard_identity_release(NULL);
}

/**
 * The kit's `vectors/standard/identity` vectors, one row each and named for the vector: a row with `other` set is an
 * order vector, any other row answers `valid` and, refused, the offset.
 */
typedef struct kit_vector {
    const char *id;
    const char *value;
    const char *other;
    int valid;
    size_t offset;
    int order;
} kit_vector;

static const kit_vector KIT_VECTORS[] = {
    {"accepts-a-local-id", "local:plugin-stub", NULL, 1, 0, 0},
    {"accepts-a-path-under-a-git-id", "git:" SHA1_ROOT "/kernel", NULL, 1, 0, 0},
    {"accepts-a-segment-containing-two-dots", "git:" SHA1_ROOT "/a..b", NULL, 1, 0, 0},
    {"accepts-a-segment-starting-with-a-dot", "git:" SHA1_ROOT "/.hidden", NULL, 1, 0, 0},
    {"accepts-a-sha1-git-id", "git:" SHA1_ROOT, NULL, 1, 0, 0},
    {"accepts-a-sha256-git-id", "git:" SHA256_ROOT, NULL, 1, 0, 0},
    {"orders-a-prefix-first", "git:" SAME_ROOT, "git:" SAME_ROOT "/a", 1, 0, -1},
    {"orders-by-segments-after-the-id", "git:" SAME_ROOT "/a", "git:" SAME_ROOT "/b", 1, 0, -1},
    {"orders-equal-ids-as-equal", "git:" SAME_ROOT "/a", "git:" SAME_ROOT "/a", 1, 0, 0},
    {"orders-git-ids-by-hex-text", "git:" SAME_ROOT, "git:" LATER_ROOT, 1, 0, -1},
    {"orders-local-before-git", "local:z", "git:" SAME_ROOT, 1, 0, -1},
    {"refuses-41-hex-digits", "git:" SHA1_ROOT "0", NULL, 0, 44, 0},
    {"refuses-a-bare-name", "calendar", NULL, 0, 0, 0},
    {"refuses-a-dot-dot-segment-before-a-name", "git:" SHA1_ROOT "/../calendar", NULL, 0, 45, 0},
    {"refuses-a-dot-dot-segment-between-names", "git:" SHA1_ROOT "/a/../b", NULL, 0, 47, 0},
    {"refuses-a-dot-dot-segment", "git:" SHA1_ROOT "/..", NULL, 0, 45, 0},
    {"refuses-a-forge-number-id", "forge:123", NULL, 0, 0, 0},
    {"refuses-a-local-name-with-whitespace", "local:a b", NULL, 0, 7, 0},
    {"refuses-uppercase-hex", "git:4B825DC642CB6EB9A060E54BF8D69288FBEE4904", NULL, 0, 5, 0},
};

static void answers_every_kit_vector(void) {
    size_t at;
    for (at = 0; at < sizeof KIT_VECTORS / sizeof KIT_VECTORS[0]; at++) {
        const kit_vector *vector = &KIT_VECTORS[at];
        if (vector->other != NULL) {
            CHECK(order_of_texts(vector->value, vector->other) == vector->order);
        } else if (vector->valid) {
            weivel_standard_identity identity = WEIVEL_INIT(weivel_standard_identity);
            CHECK(parse_text(vector->value, &identity, NULL) == WEIVEL_STATUS_OK);
            weivel_standard_identity_release(&identity);
        } else {
            check_refused_at(vector->id, vector->value, strlen(vector->value), vector->offset);
        }
    }
}

static void check_parse_agrees_with_matcher(const char *text, size_t length, int expected_valid) {
    weivel_standard_identity identity = WEIVEL_INIT(weivel_standard_identity);
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    weivel_status status = weivel_standard_identity_parse(text, length, &identity, &refusal);
    int matches = weivel_standard_matches_repository_id(text, length);
    if (matches != expected_valid) {
        fprintf(stderr, "%s: the matcher answered %d, expected %d\n", text, matches, expected_valid);
        check_failures++;
    }
    if ((status == WEIVEL_STATUS_OK) != matches) {
        fprintf(stderr, "%s: parse answered %d but the matcher answered %d\n", text, (int)status, matches);
        check_failures++;
    }
    if (status == WEIVEL_STATUS_OK) {
        weivel_standard_identity_release(&identity);
    } else {
        CHECK(status == WEIVEL_STATUS_REFUSED && refusal.code == WEIVEL_STANDARD_REFUSAL_INVALID_VALUE);
    }
    weivel_standard_refusal_release(&refusal);
}

static void parse_agrees_with_every_pattern_sample(void) {
    size_t at;
    for (at = 0; at < sizeof REPOSITORY_IDS / sizeof REPOSITORY_IDS[0]; at++) {
        check_parse_agrees_with_matcher(REPOSITORY_IDS[at].text, REPOSITORY_IDS[at].length,
                                        REPOSITORY_IDS[at].matches);
    }
}

int main(void) {
    parts_of_git_identities();
    parts_of_local_identities();
    compare_orders_by_parts();
    a_refusal_says_where_it_failed();
    a_forge_number_is_only_an_invalid_value();
    a_long_local_name_is_refused_in_linear_time();
    malformed_calls_and_missing_refusal();
    answers_every_kit_vector();
    parse_agrees_with_every_pattern_sample();
    return check_summary("identity_test");
}
