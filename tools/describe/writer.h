#ifndef WEIVEL_DESCRIBE_WRITER_H
#define WEIVEL_DESCRIBE_WRITER_H

/**
 * @file
 * The writer of weivel-describe: one C source per service, from the model alone.
 *
 * The source includes `weivel/description.h` and every scanned header, and defines `<symbol>`, answering a static
 * weivel_standard_description of the service's definition and of every struct and enum it reaches. Each struct's
 * member and first shape, each operation's offset and each enum entry's number is written as `offsetof`, `sizeof` or
 * the entry's own constant, so the compiler of the library fills them, and a member the headers no longer declare as
 * read is a compile error. Every initializer is designated and complete, so the source compiles without a warning
 * under every warning set the standard builds with.
 *
 * Every file-scope name the source declares other than `<symbol>` is `<prefix>` followed by `types`, `enums`,
 * `operations`, `blocking`, `definitions`, `description`, `filled`, `fill`, `<struct>_members`, `<enum>_entries` or
 * `<operation>_parameters`, so a prefix the library owns keeps them apart from every name its headers declare. A
 * definition a DEFINITION names that the headers declare is described in the same source, once, its names taking
 * `<prefix><definition>_` as their prefix; a struct named so that two spellings meet is a compile error, never a
 * silent merge. One described elsewhere is declared as `<definition>_description` and called once, on the symbol's
 * first call, through the platform's once primitive (`InitOnceExecuteOnce` on Windows, `pthread_once` elsewhere),
 * since C11's `call_once` is optional and macOS does not ship it; that is the only slot written after load.
 */

#include <stddef.h>

#include "model.h"

typedef struct describe_service {
    /** The definition struct to describe. */
    const char *definition;
    /** The function the source defines, answering the description. */
    const char *symbol;
    /** The identifier prefix of every other name the source declares. */
    const char *prefix;
    /** The macro the symbol is declared with, as a library's export macro, or NULL for none. */
    const char *export_macro;
    /** How the source includes each header, one per file of the model, in the order they were read. */
    const char *const *includes;
    size_t include_count;
} describe_service;

/** Bytes the writer made, NUL-terminated, `length` not counting the NUL. */
typedef struct describe_text {
    char *bytes;
    size_t length;
    size_t capacity;
} describe_text;

/**
 * Writes the source describing `service` into the empty `source`. REFUSED, with no file and line 0, for a symbol, a
 * prefix or an export macro that is not an identifier, or an include count other than the model's file count or an
 * include that is empty or holds a quote, a backslash or a line end; at the definition's line for a definition that
 * reaches itself through the definitions it holds or answers; and as describe_reach_of refuses. On REFUSED or
 * OUT_OF_MEMORY the source is left empty.
 */
describe_outcome describe_write(const describe_model *model, const describe_service *service, describe_text *source,
                                describe_refusal *refusal);

void describe_text_release(describe_text *text);

#endif
