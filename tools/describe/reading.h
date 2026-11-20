#ifndef WEIVEL_DESCRIBE_READING_H
#define WEIVEL_DESCRIBE_READING_H

#include <stddef.h>
#include <string.h>

#include "allocation.h"
#include "model.h"

/** Adds one header's declarations to `model`, every item spelled but none resolved yet. */
describe_outcome describe_parse(describe_model *model, const char *path, const char *text, size_t length,
                                describe_refusal *refusal);

/** Resolves every item of a model whose every header is parsed. */
describe_outcome describe_resolve(describe_model *model, const describe_names *names, describe_refusal *refusal);

/** `spelling` as C writes it, `const char *const *`; NULL when memory runs out. */
char *describe_spelling_text(const describe_spelling *spelling);

char *describe_copy_text(const char *text);

int describe_needs_growth(size_t count);
size_t describe_grown_capacity(size_t count);

/** Appends one zeroed element to a model array, pointing `slot` at it, or leaving `slot` NULL when memory runs out. */
#define DESCRIBE_APPEND(array, count, slot)                                                                  \
    do {                                                                                                     \
        (slot) = NULL;                                                                                       \
        if (describe_needs_growth(count)) {                                                                  \
            void *grown_array = weivel_standard_memory_reallocate(                                           \
                (array), describe_grown_capacity(count) * sizeof *(array));                                  \
            if (grown_array == NULL) {                                                                       \
                break;                                                                                       \
            }                                                                                                \
            (array) = grown_array;                                                                           \
        }                                                                                                    \
        memset(&(array)[count], 0, sizeof *(array));                                                         \
        (slot) = &(array)[(count)++];                                                                        \
    } while (0)

#endif
