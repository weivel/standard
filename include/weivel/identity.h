#ifndef WEIVEL_STANDARD_IDENTITY_H
#define WEIVEL_STANDARD_IDENTITY_H

#include <stddef.h>
#include <stdint.h>

#include "weivel/core.h"
#include "weivel/standard.h"
#include "weivel/standard_api.h"

typedef enum weivel_standard_identity_kind {
    WEIVEL_STANDARD_IDENTITY_LOCAL = 0,
    WEIVEL_STANDARD_IDENTITY_GIT = 1
} weivel_standard_identity_kind;

/**
 * A repository identity taken apart, owned by the caller until weivel_standard_identity_release. A `local` identity
 * holds `name` (`name_length` bytes, NUL-terminated); a git identity holds `commit`, the object id of the repository's
 * first-parent root commit as `commit_length` lowercase hex digits (40 or 64), NUL-terminated. Both hold the path
 * `segments` after the root, in order, none `.` or `..`. The fields a kind does not use are NULL or 0. An empty
 * identity has its `size` set and every other field zero, as WEIVEL_INIT or a release leaves it. First shape ends at
 * `commit_length`.
 */
typedef struct weivel_standard_identity {
    uint32_t size;
    uint32_t kind;
    char *name;
    size_t name_length;
    char **segments;
    size_t segment_count;
    char *commit;
    size_t commit_length;
} weivel_standard_identity;

/**
 * Takes `length` bytes of `text` apart as a repository identity: `local:<name>`, the name holding no whitespace, NUL or
 * `/`, or `git:<40 or 64 lowercase hex digits>`, then path segments. It accepts exactly the strings the plugin schema's
 * identity pattern accepts.
 *
 * @param identity the caller's struct with `size` set; receives the parts on WEIVEL_STATUS_OK and is left empty on
 *        every other answer. Required.
 * @param refusal receives the refusal, or may be NULL. INVALID_VALUE, the only code a parse refuses with, when the
 *        text is not an identity, its detail beginning `byte <offset>:` with the offset where it stopped being one: 0
 *        when the text starts with neither `git:` nor `local:`; past `git:`, where a run of fewer than 40 hex digits
 *        ends, at the 41st digit of a run of 41 to 63 and at the 65th of a longer one; else the first byte that cannot
 *        continue the name or a segment, or a segment's first byte when it is empty, `.` or `..`. `path` is empty.
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
 * Orders two identities: kind (local first), then a local one's name bytes or a git one's commit hex as text, then
 * the segments in order, a shorter list first when it is a prefix of the other. Returns -1, 0 or 1.
 */
WEIVEL_STANDARD_API int weivel_standard_identity_compare(const weivel_standard_identity *left,
                                                         const weivel_standard_identity *right);

#endif
