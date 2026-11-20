#ifndef WEIVEL_DESCRIBE_READER_H
#define WEIVEL_DESCRIBE_READER_H

/**
 * @file
 * The reader of weivel's description grammar. It is not a C parser: it reads the few forms a weivel definition is
 * written in and refuses everything else, so a header it accepts is described whole or not at all.
 *
 * Read and skipped: block and line comments (a `/` `**` comment documents the declaration that follows it, and a
 * preprocessor line between them drops it), preprocessor lines with their continuations (every branch of a
 * conditional is read), `extern "C" {` and its `}`, and a function declaration `<identifiers and *> name(...);`,
 * export macros included. At the top level nothing else is read but:
 *
 * - `typedef struct <name> <name>;`, an opaque handle unless the same name is given a body, which is refused;
 * - `typedef struct <name> { <members> } <name>;`;
 * - `typedef enum <name> { <entry> [= <number>], ... } <name>;`, a number being decimal or hex and below 2^32, one left
 *   out being the previous one plus one, the first 0.
 *
 * A member or a parameter is `[const] <identifier> [const] {* [const]} <name>`, resolved by kind:
 *
 * - `uint32_t`, `uint64_t`, `int64_t`, `double`, `size_t` by value; `const char *` a text; `const char *const *` texts
 *   and `const uint8_t *` bytes, each with its count; a struct of the scanned headers by value, or the value type;
 * - `const T *`, T a struct that is no definition or the value type, a POINTER, or an ARRAY when a count counts it;
 *   `const S *`, S a scalar above other than size_t, an ARRAY and never uncounted;
 * - `H *` or `const H *` for an opaque handle H, a handle input as a parameter;
 * - as a member only, `const D *`, D a definition of the scanned headers or one named as described elsewhere, a
 *   DEFINITION; a parameter answers one as `const D **`, and is refused for taking one as an input;
 * - as a member only: `void *context` in a definition and `void *storage`;
 * - as a parameter only: `void *context` first; a non-const `S *`, S a struct, an answer, or the refusal when S is the
 *   refusal type, which only the last parameter may be (an operation whose last parameter is not of the refusal
 *   type, as weivel_standard_paths' are, has no refusal); `<tree> *` a value answer; `const char **` a text answer;
 *   a non-const pointer to a scalar a scalar answer; `H **` a handle answer.
 *
 * Counts. A TEXTS or an ARRAY named N is counted by the size_t beside it named `N_count`, N less a trailing `ies`
 * then `y_count` (`entries` by `entry_count`), N less a trailing `s` then `_count` (`tags` by `tag_count`), or
 * `count`; BYTES named N by `N_length` or `length`. Exactly one may exist, a candidate that is not a size_t is
 * refused, and a size_t counts one thing at most. A size_t named `<x>_count` or `<x>_length` that counts nothing is
 * refused, so a name that misses its array never passes as an unrelated size, unless it is `<x>_length` beside a
 * text `<x>` or a parameter answered through `size_t *`, which counts nothing beside it by design.
 *
 * Enums. A uint32_t holds an enum E of the scanned headers when its own doc names E, a parameter's own doc being its
 * `@param` paragraph. Otherwise, the doc of its struct, or of its operation up to the first `@` tag, is split into
 * clauses at each `;` and at each `.` followed by a space or the end, in order; a clause naming E binds it to the one
 * uint32_t the clause names in backticks, or, when the clause names no member or parameter in backticks at all, to
 * the only uint32_t there is that holds no enum yet (a leading `size` never does). Refused as ambiguous: an own doc or
 * a clause naming two enums, a clause backticking two uint32_t, a clause backticking none when two uint32_t hold no
 * enum yet, and one uint32_t bound to two enums. A clause that backticks only items that are not uint32_t binds
 * nothing.
 *
 * Definitions. A struct with function pointers is a definition: `uint32_t size; void *context;`, then only function
 * pointers, each an operation `<status> (*name)(void *context, <parameters>)` or a release
 * `void (*name)(void *context, T *x)`. A release names the type it frees, a definition holding at most one per type.
 * A handle, input or answer, and a value tree answer always name the release of the same definition freeing them,
 * and are refused without one. A struct answer or a refusal names it when the definition has one, and is refused
 * without one only when its struct holds storage: a `void *storage` or a handle, directly or through a struct it
 * holds or points at; an answer that owns nothing has no release. A text or scalar answer has none.
 *
 * Blocking. An operation whose doc carries the tag `@blocking`, its reason after it, may block; a release carrying
 * it is refused.
 *
 * Shapes. A struct whose first member is `uint32_t size` must say `First shape ends at `<member>`` in its doc (or
 * `first shape ends at`, as after "Its"), the member or a definition's operation, two such lines agreeing; a struct
 * without a leading size must not.
 */

#include <stddef.h>

#include "model.h"

/**
 * Reads every header in `paths`, in order, into the empty `model`, and resolves it with `names`. On REFUSED the model
 * is released and the refusal filled; on OUT_OF_MEMORY both are left empty.
 */
describe_outcome describe_read(describe_model *model, const char *const *paths, size_t path_count,
                               const describe_names *names, describe_refusal *refusal);

/**
 * The same over headers already in memory: `texts[i]` of `lengths[i]` bytes, read as the file `paths[i]`, for the
 * tests and for a caller that has the bytes.
 */
describe_outcome describe_read_texts(describe_model *model, const char *const *paths, const char *const *texts,
                                     const size_t *lengths, size_t count, const describe_names *names,
                                     describe_refusal *refusal);

#endif
