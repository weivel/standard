#include "weivel/sha256.h"

#include "allocation.h"
#include "report.h"

#include <string.h>

#define BLOCK_BYTES 64
#define ROUNDS 64

static const uint32_t ROUND_CONSTANTS[ROUNDS] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static const uint32_t INITIAL_STATE[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                          0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

static uint32_t rotate_right(uint32_t value, unsigned count) {
    return (value >> count) | (value << (32 - count));
}

static void load_schedule(const uint8_t *block, uint32_t schedule[ROUNDS]) {
    unsigned at;
    for (at = 0; at < 16; at++) {
        schedule[at] = ((uint32_t)block[at * 4] << 24) | ((uint32_t)block[at * 4 + 1] << 16) |
                       ((uint32_t)block[at * 4 + 2] << 8) | (uint32_t)block[at * 4 + 3];
    }
    for (at = 16; at < ROUNDS; at++) {
        uint32_t early = schedule[at - 15];
        uint32_t late = schedule[at - 2];
        uint32_t sigma0 = rotate_right(early, 7) ^ rotate_right(early, 18) ^ (early >> 3);
        uint32_t sigma1 = rotate_right(late, 17) ^ rotate_right(late, 19) ^ (late >> 10);
        schedule[at] = schedule[at - 16] + sigma0 + schedule[at - 7] + sigma1;
    }
}

static void compress(uint32_t state[8], const uint8_t *block) {
    uint32_t schedule[ROUNDS];
    uint32_t working[8];
    unsigned round;
    load_schedule(block, schedule);
    memcpy(working, state, sizeof working);
    for (round = 0; round < ROUNDS; round++) {
        uint32_t sum1 = rotate_right(working[4], 6) ^ rotate_right(working[4], 11) ^ rotate_right(working[4], 25);
        uint32_t choice = (working[4] & working[5]) ^ (~working[4] & working[6]);
        uint32_t first = working[7] + sum1 + choice + ROUND_CONSTANTS[round] + schedule[round];
        uint32_t sum0 = rotate_right(working[0], 2) ^ rotate_right(working[0], 13) ^ rotate_right(working[0], 22);
        uint32_t majority = (working[0] & working[1]) ^ (working[0] & working[2]) ^ (working[1] & working[2]);
        uint32_t second = sum0 + majority;
        memmove(working + 1, working, 7 * sizeof working[0]);
        working[4] += first;
        working[0] = first + second;
    }
    for (round = 0; round < 8; round++) {
        state[round] += working[round];
    }
}

/** The one or two blocks that close a message: its last partial block, the 0x80 marker, zeros, the bit length. */
static size_t build_tail(const uint8_t *rest, size_t rest_length, uint64_t bit_length, uint8_t tail[2 * BLOCK_BYTES]) {
    size_t tail_length = rest_length + 1 + 8 <= BLOCK_BYTES ? BLOCK_BYTES : 2 * BLOCK_BYTES;
    unsigned at;
    memset(tail, 0, 2 * BLOCK_BYTES);
    if (rest_length > 0) {
        memcpy(tail, rest, rest_length);
    }
    tail[rest_length] = 0x80;
    for (at = 0; at < 8; at++) {
        tail[tail_length - 1 - at] = (uint8_t)(bit_length >> (8 * at));
    }
    return tail_length;
}

void weivel_standard_sha256(const void *bytes, size_t length, uint8_t out[WEIVEL_STANDARD_SHA256_BYTES]) {
    uint32_t state[8];
    const uint8_t *cursor = bytes;
    size_t whole_blocks = length / BLOCK_BYTES;
    size_t at;
    uint8_t tail[2 * BLOCK_BYTES];
    size_t tail_length;
    memcpy(state, INITIAL_STATE, sizeof state);
    for (at = 0; at < whole_blocks; at++) {
        compress(state, cursor + at * BLOCK_BYTES);
    }
    tail_length = build_tail(cursor + whole_blocks * BLOCK_BYTES, length % BLOCK_BYTES, (uint64_t)length * 8, tail);
    for (at = 0; at < tail_length; at += BLOCK_BYTES) {
        compress(state, tail + at);
    }
    for (at = 0; at < 8; at++) {
        out[at * 4] = (uint8_t)(state[at] >> 24);
        out[at * 4 + 1] = (uint8_t)(state[at] >> 16);
        out[at * 4 + 2] = (uint8_t)(state[at] >> 8);
        out[at * 4 + 3] = (uint8_t)state[at];
    }
}

void weivel_standard_sha256_hex(const void *bytes, size_t length, char out[WEIVEL_STANDARD_SHA256_HEX_LENGTH + 1]) {
    static const char DIGITS[] = "0123456789abcdef";
    uint8_t digest[WEIVEL_STANDARD_SHA256_BYTES];
    size_t at;
    weivel_standard_sha256(bytes, length, digest);
    for (at = 0; at < WEIVEL_STANDARD_SHA256_BYTES; at++) {
        out[at * 2] = DIGITS[digest[at] >> 4];
        out[at * 2 + 1] = DIGITS[digest[at] & 15];
    }
    out[WEIVEL_STANDARD_SHA256_HEX_LENGTH] = '\0';
}

int weivel_standard_is_sha256_hex(const char *text) {
    size_t at;
    if (text == NULL) {
        return 0;
    }
    for (at = 0; at < WEIVEL_STANDARD_SHA256_HEX_LENGTH; at++) {
        char character = text[at];
        if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f'))) {
            return 0;
        }
    }
    return text[WEIVEL_STANDARD_SHA256_HEX_LENGTH] == '\0';
}

static weivel_status refuse_download(weivel_standard_refusal *refusal, char *detail, char *fix) {
    weivel_standard_refusal_texts texts;
    texts.detail = detail;
    texts.fix = fix;
    texts.path = weivel_standard_format_text("%s", "");
    if (refusal == NULL) {
        weivel_standard_release_texts(&texts);
        return detail != NULL && fix != NULL ? WEIVEL_STATUS_REFUSED : WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    return weivel_standard_fill_refusal(refusal, WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, texts)
               ? WEIVEL_STATUS_REFUSED
               : WEIVEL_STATUS_OUT_OF_MEMORY;
}

static weivel_status refuse_expected_hash(weivel_standard_refusal *refusal) {
    return refuse_download(
        refusal, weivel_standard_format_text("the expected hash is not 64 lowercase hexadecimal characters"),
        weivel_standard_format_text("pass the SHA-256 as 64 characters of 0 to 9 and a to f, lowercase"));
}

static weivel_status refuse_mismatch(weivel_standard_refusal *refusal, const char *expected_hex,
                                     const char *actual_hex) {
    return refuse_download(
        refusal,
        weivel_standard_format_text("the download's SHA-256 is %s, not the expected %s", actual_hex, expected_hex),
        weivel_standard_format_text("discard the download and fetch it from another mirror"));
}

weivel_status weivel_standard_verify_download(const void *bytes, size_t length, const char *expected_hex,
                                              weivel_standard_refusal *refusal) {
    char actual_hex[WEIVEL_STANDARD_SHA256_HEX_LENGTH + 1];
    if (weivel_standard_refusal_holds_one(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    if (expected_hex == NULL) {
        return weivel_standard_refuse_call(refusal, "weivel_standard_verify_download was given no expected hash",
                                           "pass the expected SHA-256 as 64 lowercase hexadecimal characters");
    }
    if (bytes == NULL && length > 0) {
        return weivel_standard_refuse_call(refusal, "weivel_standard_verify_download was given no bytes but a length",
                                           "pass the downloaded bytes, or a length of 0");
    }
    if (!weivel_standard_is_sha256_hex(expected_hex)) {
        return refuse_expected_hash(refusal);
    }
    weivel_standard_sha256_hex(bytes, length, actual_hex);
    if (memcmp(actual_hex, expected_hex, sizeof actual_hex) == 0) {
        return WEIVEL_STATUS_OK;
    }
    return refuse_mismatch(refusal, expected_hex, actual_hex);
}
