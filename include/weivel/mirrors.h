#ifndef WEIVEL_STANDARD_MIRRORS_H
#define WEIVEL_STANDARD_MIRRORS_H

#include <stddef.h>

#include "weivel/core.h"
#include "weivel/standard.h"
#include "weivel/standard_api.h"

/** The placeholder every mirror template carries exactly once. */
#define WEIVEL_STANDARD_MIRROR_PLACEHOLDER "{sha256}"

/**
 * An ordered list of places to fetch the core's binary from, as URL templates. The standard ships none: the list is
 * a user's file, and a user replaces it. Everything lives in `storage`, private to the standard, until
 * weivel_standard_mirror_list_release. An empty list is all zero, as a zeroed struct or a release leaves it.
 */
typedef struct weivel_standard_mirror_list {
    /** The templates in file order, each holding the placeholder once. */
    const char *const *templates;
    size_t count;
    void *storage;
} weivel_standard_mirror_list;

/**
 * Reads `{ "mirrors": [ "<template>", ... ] }`. A template is a string holding WEIVEL_STANDARD_MIRROR_PLACEHOLDER
 * exactly once; the list is not empty. A field the format does not define is ignored.
 *
 * @param json the document, read only during the call; it need not be NUL-terminated. Required.
 * @param list the caller's empty list; filled on WEIVEL_STATUS_OK and left empty on every other answer. Required.
 * @param refusal the caller's empty refusal, or NULL for none; filled on WEIVEL_STATUS_REFUSED with the field's
 * `path`, as `mirrors[1]`, and on WEIVEL_STATUS_MALFORMED.
 * @return WEIVEL_STATUS_OK; WEIVEL_STATUS_REFUSED for malformed JSON (path "") or a field the rules refuse;
 * WEIVEL_STATUS_MALFORMED for a NULL json or list, or a list or refusal still holding one, which is left as it was;
 * WEIVEL_STATUS_OUT_OF_MEMORY, with nothing allocated.
 */
WEIVEL_STANDARD_API weivel_status weivel_standard_read_mirrors(const char *json, size_t length,
                                                               weivel_standard_mirror_list *list,
                                                               weivel_standard_refusal *refusal);

/**
 * Reads the mirror list in the file at `path` (UTF-8, a link is not followed, at most 1 MiB) as
 * weivel_standard_read_mirrors does. A refusal for the file itself is NOT_FOUND, or UNREADABLE for one that cannot be
 * read or is too large.
 */
WEIVEL_STANDARD_API weivel_status weivel_standard_load_mirrors(const char *path, weivel_standard_mirror_list *list,
                                                               weivel_standard_refusal *refusal);

/** Frees what a list holds and leaves it empty; a no-op for NULL or an empty one. */
WEIVEL_STANDARD_API void weivel_standard_mirror_list_release(weivel_standard_mirror_list *list);

/**
 * Expands `template_text` for a download: the placeholder becomes `hash_hex`, which must be 64 lowercase hex
 * characters.
 *
 * @param url on WEIVEL_STATUS_OK an owned NUL-terminated string, freed with weivel_standard_text_release; NULL
 * otherwise. Required.
 * @return WEIVEL_STATUS_OK; WEIVEL_STATUS_REFUSED for a hash that is not 64 lowercase hex characters or a template
 * without exactly one placeholder (INVALID_VALUE); WEIVEL_STATUS_MALFORMED for a NULL argument or a refusal still
 * holding one; WEIVEL_STATUS_OUT_OF_MEMORY.
 */
WEIVEL_STANDARD_API weivel_status weivel_standard_mirror_expand(const char *template_text, const char *hash_hex,
                                                                char **url, weivel_standard_refusal *refusal);

/** Frees a string the standard returned; a no-op for NULL. */
WEIVEL_STANDARD_API void weivel_standard_text_release(char *text);

#endif
