#ifndef WEIVEL_STANDARD_IDENTITY_H
#define WEIVEL_STANDARD_IDENTITY_H

#include <stddef.h>
#include <stdint.h>

#include "weivel/core.h"
#include "weivel/standard.h"
#include "weivel/standard_api.h"

typedef enum weivel_standard_identity_kind {
    WEIVEL_STANDARD_IDENTITY_LOCAL = 0,
    WEIVEL_STANDARD_IDENTITY_FORGE = 1
} weivel_standard_identity_kind;

/**
 * A repository identity taken apart, owned by the caller until weivel_standard_identity_release. A `local` identity
 * holds `name` (`name_length` bytes, NUL-terminated, which may itself hold a NUL); a forge identity holds `forge`
 * (an opaque word), `host` (NULL when the identity names none), and the repository `number`. Both hold the path
 * `segments` after the root, in order, none `.` or `..`. The fields a kind does not use are NULL or 0. An empty identity
 * has its `size` set and every other field zero, as WEIVEL_INIT or a release leaves it.
 */
typedef struct weivel_standard_identity {
    uint32_t size;
    uint32_t kind;
    char *name;
    size_t name_length;
    char *forge;
    char *host;
    uint64_t number;
    char **segments;
    size_t segment_count;
} weivel_standard_identity;

/**
 * Takes `length` bytes of `text` apart as a repository identity: `local:<name>` or `<forge>[@host]:<number>`, then
 * path segments. It accepts exactly the strings the plugin schema's identity pattern accepts.
 *
 * @param identity the caller's struct with `size` set; receives the parts on WEIVEL_STATUS_OK and is left empty on
 *        every other answer. Required.
 * @param refusal receives the refusal, or may be NULL. INVALID_VALUE when the text is not an identity, its detail
 *        giving the byte offset where it stopped being one; UNREPRESENTABLE when the repository number does not fit
 *        64 bits. `path` is empty.
 * @return WEIVEL_STATUS_OK; WEIVEL_STATUS_REFUSED; WEIVEL_STATUS_MALFORMED for a NULL `text` or `identity`, an
 *         identity below its first shape, or a refusal below its first shape or still holding one;
 *         WEIVEL_STATUS_OUT_OF_MEMORY.
 */
WEIVEL_STANDARD_API weivel_status weivel_standard_identity_parse(const char *text, size_t length,
                                                                 weivel_standard_identity *identity,
                                                                 weivel_standard_refusal *refusal);

/** Frees what an identity holds and leaves it empty with its `size` kept; a no-op for NULL. */
WEIVEL_STANDARD_API void weivel_standard_identity_release(weivel_standard_identity *identity);

/**
 * Orders two identities by their parts: kind (local first), then for a local one its name bytes, for a forge one the
 * forge word, the host (none before any), and the number as a number; then the segments in order, a shorter list
 * first when it is a prefix of the other. Leading zeros do not matter: `forge:007` and `forge:7` are one repository
 * and compare 0. Returns below zero, zero or above zero, so `forge:9` < `forge:10`.
 */
WEIVEL_STANDARD_API int weivel_standard_identity_compare(const weivel_standard_identity *left,
                                                         const weivel_standard_identity *right);

#endif
