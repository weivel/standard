#ifndef WEIVEL_STANDARD_SHA256_H
#define WEIVEL_STANDARD_SHA256_H

#include <stddef.h>
#include <stdint.h>

#include "weivel/core.h"
#include "weivel/standard.h"
#include "weivel/standard_api.h"

/** The length of a SHA-256 digest in bytes, and of its lowercase hexadecimal form without the NUL. */
#define WEIVEL_STANDARD_SHA256_BYTES 32
#define WEIVEL_STANDARD_SHA256_HEX_LENGTH 64

/** The SHA-256 (FIPS 180-4) of `length` bytes; `bytes` may be NULL only when `length` is 0. */
WEIVEL_STANDARD_API void weivel_standard_sha256(const void *bytes, size_t length,
                                                uint8_t out[WEIVEL_STANDARD_SHA256_BYTES]);

/** The digest as 64 lowercase hex characters and a NUL, into `out`. */
WEIVEL_STANDARD_API void weivel_standard_sha256_hex(const void *bytes, size_t length,
                                                    char out[WEIVEL_STANDARD_SHA256_HEX_LENGTH + 1]);

/** 1 when `text` is exactly 64 lowercase hexadecimal characters, else 0; uppercase is not accepted. */
WEIVEL_STANDARD_API int weivel_standard_is_sha256_hex(const char *text);

/**
 * Checks that a downloaded buffer is the one that was asked for. The standard never fetches: the caller downloads,
 * then asks.
 *
 * @param bytes the download, read only during the call; may be NULL when `length` is 0.
 * @param expected_hex the expected SHA-256, 64 lowercase hex characters. Required.
 * @param refusal the caller's empty refusal, or NULL for none; filled on every answer but OK.
 * @return WEIVEL_STATUS_OK on a match; WEIVEL_STATUS_REFUSED on a mismatch (INVALID_VALUE, the detail naming both
 * hashes) or an `expected_hex` that is not 64 lowercase hex characters; WEIVEL_STATUS_MALFORMED for a NULL
 * `expected_hex`, NULL `bytes` with a length, or a refusal below its first shape or still holding one;
 * WEIVEL_STATUS_OUT_OF_MEMORY.
 */
WEIVEL_STANDARD_API weivel_status weivel_standard_verify_download(const void *bytes, size_t length,
                                                                  const char *expected_hex,
                                                                  weivel_standard_refusal *refusal);

#endif
