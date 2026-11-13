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

static size_t path_length(const weivel_standard_field_path *path) {
    size_t before;
    if (path == NULL) {
        return 0;
    }
    before = path_length(path->parent);
    if (path->key == NULL) {
        return before + index_digits(path->index) + 2;
    }
    return before + (before > 0 ? 1 : 0) + strlen(path->key);
}

static char *path_write(const weivel_standard_field_path *path, char *cursor, const char *start) {
    if (path == NULL) {
        return cursor;
    }
    cursor = path_write(path->parent, cursor, start);
    if (path->key == NULL) {
        return cursor + snprintf(cursor, index_digits(path->index) + 3, "[%zu]", path->index);
    }
    if (cursor != start) {
        *cursor++ = '.';
    }
    memcpy(cursor, path->key, strlen(path->key));
    return cursor + strlen(path->key);
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
