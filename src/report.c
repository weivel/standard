#include "report.h"

#include "allocation.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static size_t index_digits(size_t index) {
    size_t digits = 1;
    while (index >= 10) {
        index /= 10;
        digits++;
    }
    return digits;
}

static int is_plain_key_character(char character) {
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
           (character >= '0' && character <= '9') || character == '_' || character == '-';
}

static int is_plain_key(const char *key) {
    if (key[0] == '\0') {
        return 0;
    }
    for (; *key != '\0'; key++) {
        if (!is_plain_key_character(*key)) {
            return 0;
        }
    }
    return 1;
}

static const char *short_escape(unsigned char character) {
    switch (character) {
    case '"':
        return "\\\"";
    case '\\':
        return "\\\\";
    case '\b':
        return "\\b";
    case '\f':
        return "\\f";
    case '\n':
        return "\\n";
    case '\r':
        return "\\r";
    case '\t':
        return "\\t";
    default:
        return NULL;
    }
}

/** Writes one key character as JSON escapes it, when `cursor` is not NULL, and answers how many bytes it takes. */
static size_t write_escaped_character(unsigned char character, char *cursor) {
    const char *escape = short_escape(character);
    if (escape != NULL) {
        if (cursor != NULL) {
            memcpy(cursor, escape, 2);
        }
        return 2;
    }
    if (character < 0x20) {
        if (cursor != NULL) {
            snprintf(cursor, 7, "\\u%04x", character);
        }
        return 6;
    }
    if (cursor != NULL) {
        *cursor = (char)character;
    }
    return 1;
}

/** A key segment as `.key`, or `key` first; any other key as `["<JSON-escaped key>"]`. Writes when `cursor` is set. */
static size_t write_key(const char *key, int first, char *cursor) {
    size_t written = 0;
    const unsigned char *at;
    if (is_plain_key(key)) {
        size_t length = strlen(key);
        if (!first) {
            if (cursor != NULL) {
                cursor[written] = '.';
            }
            written++;
        }
        if (cursor != NULL) {
            memcpy(cursor + written, key, length);
        }
        return written + length;
    }
    if (cursor != NULL) {
        memcpy(cursor, "[\"", 2);
    }
    written = 2;
    for (at = (const unsigned char *)key; *at != '\0'; at++) {
        written += write_escaped_character(*at, cursor == NULL ? NULL : cursor + written);
    }
    if (cursor != NULL) {
        memcpy(cursor + written, "\"]", 2);
    }
    return written + 2;
}

static size_t path_length(const weivel_standard_field_path *path) {
    size_t before;
    if (path == NULL) {
        return 0;
    }
    before = path_length(path->parent);
    if (path->key == NULL) {
        return before + index_digits(path->index) + 2;
    }
    return before + write_key(path->key, before == 0, NULL);
}

static char *path_write(const weivel_standard_field_path *path, char *cursor, const char *start) {
    if (path == NULL) {
        return cursor;
    }
    cursor = path_write(path->parent, cursor, start);
    if (path->key == NULL) {
        return cursor + snprintf(cursor, index_digits(path->index) + 3, "[%zu]", path->index);
    }
    return cursor + write_key(path->key, cursor == start, cursor);
}

char *weivel_standard_path_text(const weivel_standard_field_path *path) {
    size_t length = path_length(path);
    char *text = weivel_standard_memory_allocate(length + 1);
    if (text != NULL) {
        *path_write(path, text, text) = '\0';
    }
    return text;
}

char *weivel_standard_format_text(const char *format, ...) {
    va_list arguments;
    int length;
    char *text;
    va_start(arguments, format);
    length = vsnprintf(NULL, 0, format, arguments);
    va_end(arguments);
    if (length < 0) {
        return NULL;
    }
    text = weivel_standard_memory_allocate((size_t)length + 1);
    if (text == NULL) {
        return NULL;
    }
    va_start(arguments, format);
    vsnprintf(text, (size_t)length + 1, format, arguments);
    va_end(arguments);
    return text;
}

void weivel_standard_release_texts(weivel_standard_refusal_texts *texts) {
    weivel_standard_memory_release(texts->detail);
    weivel_standard_memory_release(texts->fix);
    weivel_standard_memory_release(texts->path);
}

int weivel_standard_fill_refusal(weivel_standard_refusal *refusal, uint32_t code,
                                 weivel_standard_refusal_texts texts) {
    weivel_standard_refusal_texts *storage;
    if (texts.detail == NULL || texts.fix == NULL || texts.path == NULL) {
        weivel_standard_release_texts(&texts);
        return 0;
    }
    storage = weivel_standard_memory_allocate(sizeof *storage);
    if (storage == NULL) {
        weivel_standard_release_texts(&texts);
        return 0;
    }
    *storage = texts;
    refusal->code = code;
    refusal->detail = storage->detail;
    refusal->fix = storage->fix;
    refusal->path = storage->path;
    refusal->storage = storage;
    return 1;
}

weivel_status weivel_standard_refuse_malformed_json(weivel_standard_refusal *refusal, const weivel_json_error *error) {
    weivel_standard_refusal_texts texts = {NULL, NULL, NULL};
    if (refusal == NULL) {
        return WEIVEL_STATUS_REFUSED;
    }
    texts.detail = weivel_standard_format_text("line %d, column %d: %s", error->line, error->column,
                                               error->message == NULL ? "malformed JSON" : error->message);
    texts.fix = weivel_standard_format_text("correct the JSON at line %d, column %d", error->line, error->column);
    texts.path = weivel_standard_format_text("%s", "");
    return weivel_standard_fill_refusal(refusal, WEIVEL_STANDARD_REFUSAL_MALFORMED_JSON, texts)
               ? WEIVEL_STATUS_REFUSED
               : WEIVEL_STATUS_OUT_OF_MEMORY;
}

weivel_status weivel_standard_refuse_call(weivel_standard_refusal *refusal, const char *detail, const char *fix) {
    weivel_standard_refusal_texts texts = {NULL, NULL, NULL};
    if (refusal == NULL) {
        return WEIVEL_STATUS_MALFORMED;
    }
    texts.detail = weivel_standard_format_text("%s", detail);
    texts.fix = weivel_standard_format_text("%s", fix);
    texts.path = weivel_standard_format_text("%s", "");
    return weivel_standard_fill_refusal(refusal, WEIVEL_STANDARD_REFUSAL_MALFORMED_CALL, texts)
               ? WEIVEL_STATUS_MALFORMED
               : WEIVEL_STATUS_OUT_OF_MEMORY;
}

int weivel_standard_refusal_holds_one(const weivel_standard_refusal *refusal) {
    return refusal != NULL && (refusal->storage != NULL || refusal->code != WEIVEL_STANDARD_REFUSAL_NONE);
}

const char *weivel_standard_subject_of(const char *path) {
    return path[0] == '\0' ? "the manifest" : path;
}

const char *weivel_standard_kind_phrase(const weivel_json_value *value) {
    switch (weivel_json_kind_of(value)) {
    case WEIVEL_JSON_BOOL:
        return "a boolean";
    case WEIVEL_JSON_NUMBER:
        return "a number";
    case WEIVEL_JSON_STRING:
        return "a string";
    case WEIVEL_JSON_ARRAY:
        return "an array";
    case WEIVEL_JSON_OBJECT:
        return "an object";
    default:
        return "null";
    }
}

int weivel_standard_is_whole_number(double number) {
    return number <= -9.2e18 || number >= 9.2e18 || (double)(int64_t)number == number;
}

void weivel_standard_refusal_release(weivel_standard_refusal *refusal) {
    weivel_standard_refusal empty = {0};
    if (refusal == NULL) {
        return;
    }
    if (refusal->storage != NULL) {
        weivel_standard_refusal_texts *storage = refusal->storage;
        weivel_standard_release_texts(storage);
        weivel_standard_memory_release(storage);
    }
    *refusal = empty;
}
