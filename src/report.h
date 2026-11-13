#ifndef WEIVEL_STANDARD_REPORT_H
#define WEIVEL_STANDARD_REPORT_H

#include <stddef.h>
#include <stdint.h>

#include "weivel/json.h"
#include "weivel/standard.h"

/** One step from the document's root to a field: a member `key`, or, when `key` is NULL, the item at `index`. */
typedef struct weivel_standard_field_path {
    const struct weivel_standard_field_path *parent;
    const char *key;
    size_t index;
} weivel_standard_field_path;

typedef struct weivel_standard_refusal_texts {
    char *detail;
    char *fix;
    char *path;
} weivel_standard_refusal_texts;

/** The path as `services.consumes[1].provider`, empty for the root; NULL when memory runs out. */
char *weivel_standard_path_text(const weivel_standard_field_path *path);

/** A printf-formatted copy; NULL when memory runs out. */
char *weivel_standard_format_text(const char *format, ...);

void weivel_standard_release_texts(weivel_standard_refusal_texts *texts);

/** Takes the three texts whatever it answers: 1 with the refusal filled, 0 when memory ran out first. */
int weivel_standard_fill_refusal(weivel_standard_refusal *refusal, uint32_t code,
                                 weivel_standard_refusal_texts texts);

/** How a field is named in a sentence: its path, or "the manifest" for the root. */
const char *weivel_standard_subject_of(const char *path);

/** The JSON type of a value as a sentence names it, "a number" or "null". */
const char *weivel_standard_kind_phrase(const weivel_json_value *value);

/** A whole number in any spelling JSON allows (`2`, `2.0`, `2e0`), as draft-07 counts an integer. */
int weivel_standard_is_whole_number(double number);

#endif
