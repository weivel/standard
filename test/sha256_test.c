#include "check.h"
#include "weivel/sha256.h"

#include <stdlib.h>

static const char *const EMPTY = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
static const char *const ABC = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
static const char *const BITS_448 = "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1";
static const char *const BITS_896 = "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1";
static const char *const MILLION_A = "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0";

static void check_hash(const void *bytes, size_t length, const char *expected, const char *label) {
    char hex[WEIVEL_STANDARD_SHA256_HEX_LENGTH + 1];
    weivel_standard_sha256_hex(bytes, length, hex);
    check_text(hex, expected, label);
}

static void matches_the_nist_vectors(void) {
    const char *message_448 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    const char *message_896 =
        "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";
    char *million = malloc(1000000);
    check_hash(NULL, 0, EMPTY, "empty");
    check_hash("abc", 3, ABC, "abc");
    check_hash(message_448, strlen(message_448), BITS_448, "448 bits");
    check_hash(message_896, strlen(message_896), BITS_896, "896 bits");
    CHECK(strlen(message_448) == 56 && strlen(message_896) == 112);
    if (million != NULL) {
        memset(million, 'a', 1000000);
        check_hash(million, 1000000, MILLION_A, "one million a");
        free(million);
    } else {
        CHECK(0);
    }
}

typedef struct boundary_case {
    size_t length;
    const char *expected;
} boundary_case;

/** Expected values come from Python's hashlib.sha256 over bytes(i % 251 for i in range(length)). */
static const boundary_case BOUNDARIES[] = {
    {55, "463eb28e72f82e0a96c0a4cc53690c571281131f672aa229e0d45ae59b598b59"},
    {56, "da2ae4d6b36748f2a318f23e7ab1dfdf45acdc9d049bd80e59de82a60895f562"},
    {63, "29af2686fd53374a36b0846694cc342177e428d1647515f078784d69cdb9e488"},
    {64, "fdeab9acf3710362bd2658cdc9a29e8f9c757fcf9811603a8c447cd1d9151108"},
    {65, "4bfd2c8b6f1eec7a2afeb48b934ee4b2694182027e6d0fc075074f2fabb31781"},
    {119, "da18797ed7c3a777f0847f429724a2d8cd5138e6ed2895c3fa1a6d39d18f7ec6"},
    {120, "f52b23db1fbb6ded89ef42a23ce0c8922c45f25c50b568a93bf1c075420bbb7c"}};

static void matches_the_padding_boundaries(void) {
    unsigned char bytes[128];
    size_t at;
    for (at = 0; at < sizeof bytes; at++) {
        bytes[at] = (unsigned char)(at % 251);
    }
    for (at = 0; at < sizeof BOUNDARIES / sizeof BOUNDARIES[0]; at++) {
        check_hash(bytes, BOUNDARIES[at].length, BOUNDARIES[at].expected, "boundary");
    }
}

static void answers_the_raw_digest(void) {
    uint8_t digest[WEIVEL_STANDARD_SHA256_BYTES];
    weivel_standard_sha256("abc", 3, digest);
    CHECK(digest[0] == 0xba && digest[1] == 0x78 && digest[31] == 0xad);
}

static void recognises_hex(void) {
    CHECK(weivel_standard_is_sha256_hex(ABC));
    CHECK(!weivel_standard_is_sha256_hex(NULL));
    CHECK(!weivel_standard_is_sha256_hex(""));
    CHECK(!weivel_standard_is_sha256_hex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015AD"));
    CHECK(!weivel_standard_is_sha256_hex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015a"));
    CHECK(!weivel_standard_is_sha256_hex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad0"));
    CHECK(!weivel_standard_is_sha256_hex("ga7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}

static void accepts_a_match(void) {
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    CHECK(weivel_standard_verify_download("abc", 3, ABC, &refusal) == WEIVEL_STATUS_OK);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_NONE && refusal.storage == NULL);
    CHECK(weivel_standard_verify_download(NULL, 0, EMPTY, NULL) == WEIVEL_STATUS_OK);
}

static void refuses_a_mismatch_naming_both_hashes(void) {
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    CHECK(weivel_standard_verify_download("abd", 3, ABC, &refusal) == WEIVEL_STATUS_REFUSED);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_INVALID_VALUE);
    CHECK(refusal.detail != NULL && strstr(refusal.detail, ABC) != NULL);
    {
        char actual[WEIVEL_STANDARD_SHA256_HEX_LENGTH + 1];
        weivel_standard_sha256_hex("abd", 3, actual);
        CHECK(refusal.detail != NULL && strstr(refusal.detail, actual) != NULL);
    }
    weivel_standard_refusal_release(&refusal);
    CHECK(weivel_standard_verify_download("abd", 3, ABC, NULL) == WEIVEL_STATUS_REFUSED);
}

static void refuses_uppercase_and_malformed_expectations(void) {
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    CHECK(weivel_standard_verify_download("abc", 3, "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD",
                                          &refusal) == WEIVEL_STATUS_REFUSED);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_INVALID_VALUE);
    weivel_standard_refusal_release(&refusal);
    CHECK(weivel_standard_verify_download("abc", 3, "abc", &refusal) == WEIVEL_STATUS_REFUSED);
    weivel_standard_refusal_release(&refusal);
}

static void refuses_a_malformed_call(void) {
    weivel_standard_refusal refusal = WEIVEL_INIT(weivel_standard_refusal);
    CHECK(weivel_standard_verify_download("abc", 3, NULL, &refusal) == WEIVEL_STATUS_MALFORMED);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_MALFORMED_CALL);
    CHECK(weivel_standard_verify_download("abc", 3, ABC, &refusal) == WEIVEL_STATUS_MALFORMED);
    weivel_standard_refusal_release(&refusal);
    CHECK(weivel_standard_verify_download(NULL, 3, ABC, &refusal) == WEIVEL_STATUS_MALFORMED);
    weivel_standard_refusal_release(&refusal);
}

int main(void) {
    matches_the_nist_vectors();
    matches_the_padding_boundaries();
    answers_the_raw_digest();
    recognises_hex();
    accepts_a_match();
    refuses_a_mismatch_naming_both_hashes();
    refuses_uppercase_and_malformed_expectations();
    refuses_a_malformed_call();
    return check_summary("sha256_test");
}
