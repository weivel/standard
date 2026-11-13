#include "weivel/json.h"

#include "allocation.h"

#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXIMUM_DEPTH 256

typedef struct {
    char *key;
    weivel_json_value *value;
} weivel_json_member;

struct weivel_json_value {
    weivel_json_kind kind;
    int boolean;
    int is_integer;
    int64_t integer;
    double real;
    char *text;
    size_t text_length;
    size_t count;
    size_t capacity;
    weivel_json_member *members;
};

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} text_buffer;

typedef struct {
    const char *text;
    size_t length;
    size_t position;
    int depth;
    int failed;
    weivel_json_error failure;
} parser;

static weivel_json_value *parse_value(parser *state);

static void locate_failure(parser *state, size_t position) {
    int line = 1;
    int column = 1;
    size_t index;
    for (index = 0; index < position && index < state->length; index++) {
        if (state->text[index] == '\n') {
            line++;
            column = 1;
        } else {
            column++;
        }
    }
    state->failure.line = line;
    state->failure.column = column;
}

static weivel_json_value *fail(parser *state, const char *message) {
    state->failed = 1;
    state->failure.message = message;
    locate_failure(state, state->position);
    return NULL;
}

static weivel_json_value *fail_out_of_memory(parser *state) {
    fail(state, "out of memory");
    state->failure.out_of_memory = 1;
    return NULL;
}

static size_t whole_character_prefix(const char *text, size_t maximum) {
    size_t length = strlen(text);
    if (length <= maximum) {
        return length;
    }
    while (maximum > 0 && ((unsigned char)text[maximum] & 0xC0) == 0x80) {
        maximum--;
    }
    return maximum;
}

static weivel_json_value *fail_naming_key(parser *state, size_t position, const char *key) {
    state->position = position;
    snprintf(state->failure.detail, sizeof(state->failure.detail), "duplicate key \"%.*s\"",
             (int)whole_character_prefix(key, 60), key);
    return fail(state, state->failure.detail);
}

static int has_more(const parser *state) {
    return state->position < state->length;
}

static char peek(const parser *state) {
    return has_more(state) ? state->text[state->position] : '\0';
}

static void skip_whitespace(parser *state) {
    while (has_more(state)) {
        char current = peek(state);
        if (current != ' ' && current != '\t' && current != '\n' && current != '\r') {
            return;
        }
        state->position++;
    }
}

static weivel_json_value *new_value(weivel_json_kind kind) {
    weivel_json_value *value = weivel_standard_memory_allocate_zeroed(1, sizeof(*value));
    if (value != NULL) {
        value->kind = kind;
    }
    return value;
}

static int buffer_append(text_buffer *buffer, const char *bytes, size_t count) {
    if (buffer->length + count + 1 > buffer->capacity) {
        size_t wanted = buffer->capacity == 0 ? 32 : buffer->capacity;
        char *grown;
        while (wanted < buffer->length + count + 1) {
            wanted *= 2;
        }
        grown = weivel_standard_memory_reallocate(buffer->data, wanted);
        if (grown == NULL) {
            return 0;
        }
        buffer->data = grown;
        buffer->capacity = wanted;
    }
    memcpy(buffer->data + buffer->length, bytes, count);
    buffer->length += count;
    buffer->data[buffer->length] = '\0';
    return 1;
}

static int append_code_point(text_buffer *buffer, unsigned long code_point) {
    char bytes[4];
    size_t count;
    if (code_point < 0x80) {
        bytes[0] = (char)code_point;
        count = 1;
    } else if (code_point < 0x800) {
        bytes[0] = (char)(0xC0 | (code_point >> 6));
        bytes[1] = (char)(0x80 | (code_point & 0x3F));
        count = 2;
    } else if (code_point < 0x10000) {
        bytes[0] = (char)(0xE0 | (code_point >> 12));
        bytes[1] = (char)(0x80 | ((code_point >> 6) & 0x3F));
        bytes[2] = (char)(0x80 | (code_point & 0x3F));
        count = 3;
    } else {
        bytes[0] = (char)(0xF0 | (code_point >> 18));
        bytes[1] = (char)(0x80 | ((code_point >> 12) & 0x3F));
        bytes[2] = (char)(0x80 | ((code_point >> 6) & 0x3F));
        bytes[3] = (char)(0x80 | (code_point & 0x3F));
        count = 4;
    }
    return buffer_append(buffer, bytes, count);
}

static int hex_digit(char digit) {
    if (digit >= '0' && digit <= '9') {
        return digit - '0';
    }
    if (digit >= 'a' && digit <= 'f') {
        return digit - 'a' + 10;
    }
    if (digit >= 'A' && digit <= 'F') {
        return digit - 'A' + 10;
    }
    return -1;
}

static int read_hex4(parser *state, unsigned long *out) {
    unsigned long accumulated = 0;
    int index;
    for (index = 0; index < 4; index++) {
        int digit = has_more(state) ? hex_digit(peek(state)) : -1;
        if (digit < 0) {
            fail(state, "expected four hex digits after \\u");
            return 0;
        }
        accumulated = accumulated * 16 + (unsigned long)digit;
        state->position++;
    }
    *out = accumulated;
    return 1;
}

static int is_high_surrogate(unsigned long unit) {
    return unit >= 0xD800 && unit <= 0xDBFF;
}

static int is_low_surrogate(unsigned long unit) {
    return unit >= 0xDC00 && unit <= 0xDFFF;
}

static int read_unicode_escape(parser *state, text_buffer *buffer) {
    unsigned long unit;
    if (!read_hex4(state, &unit)) {
        return 0;
    }
    if (is_low_surrogate(unit)) {
        fail(state, "lone low surrogate");
        return 0;
    }
    if (is_high_surrogate(unit)) {
        unsigned long low;
        if (state->position + 1 >= state->length || state->text[state->position] != '\\' ||
            state->text[state->position + 1] != 'u') {
            fail(state, "lone high surrogate");
            return 0;
        }
        state->position += 2;
        if (!read_hex4(state, &low)) {
            return 0;
        }
        if (!is_low_surrogate(low)) {
            fail(state, "high surrogate not followed by a low surrogate");
            return 0;
        }
        unit = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
    }
    if (!append_code_point(buffer, unit)) {
        fail_out_of_memory(state);
        return 0;
    }
    return 1;
}

static int read_escape(parser *state, text_buffer *buffer) {
    char escaped = peek(state);
    const char *simple = NULL;
    switch (escaped) {
        case '"': simple = "\""; break;
        case '\\': simple = "\\"; break;
        case '/': simple = "/"; break;
        case 'b': simple = "\b"; break;
        case 'f': simple = "\f"; break;
        case 'n': simple = "\n"; break;
        case 'r': simple = "\r"; break;
        case 't': simple = "\t"; break;
        default: break;
    }
    if (escaped == 'u') {
        state->position++;
        return read_unicode_escape(state, buffer);
    }
    if (simple == NULL) {
        fail(state, "invalid escape");
        return 0;
    }
    state->position++;
    if (!buffer_append(buffer, simple, 1)) {
        fail_out_of_memory(state);
        return 0;
    }
    return 1;
}

static int read_string_body(parser *state, text_buffer *buffer) {
    while (has_more(state)) {
        char current = peek(state);
        if (current == '"') {
            state->position++;
            return 1;
        }
        if ((unsigned char)current < 0x20) {
            fail(state, "control character in string");
            return 0;
        }
        state->position++;
        if (current == '\\') {
            if (!has_more(state)) {
                break;
            }
            if (!read_escape(state, buffer)) {
                return 0;
            }
        } else if (!buffer_append(buffer, &current, 1)) {
            fail_out_of_memory(state);
            return 0;
        }
    }
    fail(state, "unterminated string");
    return 0;
}

static int read_string(parser *state, text_buffer *buffer) {
    state->position++;
    if (!buffer_append(buffer, "", 0)) {
        fail_out_of_memory(state);
        return 0;
    }
    return read_string_body(state, buffer);
}

static weivel_json_value *parse_string(parser *state) {
    text_buffer buffer = {NULL, 0, 0};
    weivel_json_value *value;
    if (!read_string(state, &buffer)) {
        weivel_standard_memory_release(buffer.data);
        return NULL;
    }
    value = new_value(WEIVEL_JSON_STRING);
    if (value == NULL) {
        weivel_standard_memory_release(buffer.data);
        return fail_out_of_memory(state);
    }
    value->text = buffer.data;
    value->text_length = buffer.length;
    return value;
}

static int matches_literal(const parser *state, const char *literal) {
    size_t length = strlen(literal);
    return state->position + length <= state->length && memcmp(state->text + state->position, literal, length) == 0;
}

static weivel_json_value *parse_literal(parser *state, const char *literal, weivel_json_kind kind, int boolean) {
    weivel_json_value *value;
    if (!matches_literal(state, literal)) {
        return fail(state, "unexpected character");
    }
    value = new_value(kind);
    if (value == NULL) {
        return fail_out_of_memory(state);
    }
    state->position += strlen(literal);
    value->boolean = boolean;
    return value;
}

static int is_digit(char character) {
    return character >= '0' && character <= '9';
}

static int is_number_start(char character) {
    return character == '-' || is_digit(character);
}

static size_t scan_digits(const parser *state, size_t from) {
    size_t end = from;
    while (end < state->length && is_digit(state->text[end])) {
        end++;
    }
    return end;
}

static int scan_number_end(parser *state, size_t *end, int *has_fraction_or_exponent) {
    size_t cursor = state->position;
    size_t digits_end;
    if (cursor < state->length && state->text[cursor] == '-') {
        cursor++;
    }
    digits_end = scan_digits(state, cursor);
    if (digits_end == cursor) {
        state->position = cursor;
        fail(state, "expected a digit");
        return 0;
    }
    if (state->text[cursor] == '0' && digits_end - cursor > 1) {
        state->position = cursor + 1;
        fail(state, "leading zero in number");
        return 0;
    }
    cursor = digits_end;
    *has_fraction_or_exponent = 0;
    if (cursor < state->length && state->text[cursor] == '.') {
        digits_end = scan_digits(state, cursor + 1);
        if (digits_end == cursor + 1) {
            state->position = cursor + 1;
            fail(state, "expected a digit after the decimal point");
            return 0;
        }
        cursor = digits_end;
        *has_fraction_or_exponent = 1;
    }
    if (cursor < state->length && (state->text[cursor] == 'e' || state->text[cursor] == 'E')) {
        size_t exponent_start = cursor + 1;
        if (exponent_start < state->length && (state->text[exponent_start] == '+' || state->text[exponent_start] == '-')) {
            exponent_start++;
        }
        digits_end = scan_digits(state, exponent_start);
        if (digits_end == exponent_start) {
            state->position = exponent_start;
            fail(state, "expected a digit in the exponent");
            return 0;
        }
        cursor = digits_end;
        *has_fraction_or_exponent = 1;
    }
    *end = cursor;
    return 1;
}

static int accumulate_integer(const char *digits, int negative, int64_t *out) {
    uint64_t limit = negative ? (uint64_t)INT64_MAX + 1u : (uint64_t)INT64_MAX;
    uint64_t accumulated = 0;
    for (; *digits != '\0'; digits++) {
        uint64_t digit = (uint64_t)(*digits - '0');
        if (accumulated > (limit - digit) / 10) {
            return 0;
        }
        accumulated = accumulated * 10 + digit;
    }
    *out = negative ? (int64_t)(0 - accumulated) : (int64_t)accumulated;
    return 1;
}

static const char *decimal_point(void) {
    const struct lconv *conventions = localeconv();
    return conventions->decimal_point != NULL && conventions->decimal_point[0] != '\0' ? conventions->decimal_point : ".";
}

/**
 * strtod reads the decimal point of the host's locale, and a JSON number always uses '.', so the
 * validated text has its '.' rewritten to the locale's before conversion. The buffer holds room
 * for a decimal point longer than one byte.
 */
static double read_real(char *number) {
    const char *point = decimal_point();
    size_t point_length = strlen(point);
    char *dot = strchr(number, '.');
    if (dot != NULL && !(point_length == 1 && point[0] == '.')) {
        memmove(dot + point_length, dot + 1, strlen(dot + 1) + 1);
        memcpy(dot, point, point_length);
    }
    return strtod(number, NULL);
}

static weivel_json_value *parse_number(parser *state) {
    size_t end;
    int inexact;
    size_t length;
    char *copy;
    weivel_json_value *value;
    if (!scan_number_end(state, &end, &inexact)) {
        return NULL;
    }
    length = end - state->position;
    copy = weivel_standard_memory_allocate(length + strlen(decimal_point()) + 1);
    value = new_value(WEIVEL_JSON_NUMBER);
    if (copy == NULL || value == NULL) {
        weivel_standard_memory_release(copy);
        weivel_standard_memory_release(value);
        return fail_out_of_memory(state);
    }
    memcpy(copy, state->text + state->position, length);
    copy[length] = '\0';
    value->real = read_real(copy);
    if (!isfinite(value->real)) {
        weivel_standard_memory_release(copy);
        weivel_json_free(value);
        return fail(state, "number is not finite");
    }
    if (!inexact) {
        int negative = copy[0] == '-';
        value->is_integer = accumulate_integer(copy + negative, negative, &value->integer);
    }
    weivel_standard_memory_release(copy);
    state->position = end;
    return value;
}

static int append_member(weivel_json_value *container, char *key, weivel_json_value *member) {
    if (container->count == container->capacity) {
        size_t wanted = container->capacity == 0 ? 4 : container->capacity * 2;
        weivel_json_member *grown = weivel_standard_memory_reallocate(container->members, wanted * sizeof(*grown));
        if (grown == NULL) {
            return 0;
        }
        container->members = grown;
        container->capacity = wanted;
    }
    container->members[container->count].key = key;
    container->members[container->count].value = member;
    container->count++;
    return 1;
}

static int enter_container(parser *state) {
    if (state->depth >= MAXIMUM_DEPTH) {
        fail(state, "nesting too deep");
        return 0;
    }
    state->depth++;
    return 1;
}

static int read_object_key(parser *state, char **key) {
    text_buffer buffer = {NULL, 0, 0};
    size_t key_position = state->position;
    if (peek(state) != '"') {
        fail(state, "expected a string key");
        return 0;
    }
    if (!read_string(state, &buffer)) {
        weivel_standard_memory_release(buffer.data);
        return 0;
    }
    if (memchr(buffer.data, '\0', buffer.length) != NULL) {
        weivel_standard_memory_release(buffer.data);
        state->position = key_position;
        fail(state, "object key contains a NUL character");
        return 0;
    }
    *key = buffer.data;
    return 1;
}

static int has_member(const weivel_json_value *object, const char *key) {
    size_t index;
    for (index = 0; index < object->count; index++) {
        if (strcmp(object->members[index].key, key) == 0) {
            return 1;
        }
    }
    return 0;
}

static int read_object_member(parser *state, weivel_json_value *object) {
    char *key;
    weivel_json_value *member;
    size_t key_position = state->position;
    if (!read_object_key(state, &key)) {
        return 0;
    }
    if (has_member(object, key)) {
        fail_naming_key(state, key_position, key);
        weivel_standard_memory_release(key);
        return 0;
    }
    skip_whitespace(state);
    if (peek(state) != ':') {
        weivel_standard_memory_release(key);
        fail(state, "expected ':' after key");
        return 0;
    }
    state->position++;
    member = parse_value(state);
    if (member == NULL) {
        weivel_standard_memory_release(key);
        return 0;
    }
    if (!append_member(object, key, member)) {
        weivel_standard_memory_release(key);
        weivel_json_free(member);
        fail_out_of_memory(state);
        return 0;
    }
    return 1;
}

static int read_array_element(parser *state, weivel_json_value *array) {
    weivel_json_value *element = parse_value(state);
    if (element == NULL) {
        return 0;
    }
    if (!append_member(array, NULL, element)) {
        weivel_json_free(element);
        fail_out_of_memory(state);
        return 0;
    }
    return 1;
}

static int read_container_items(parser *state, weivel_json_value *container, char closing) {
    skip_whitespace(state);
    if (peek(state) == closing) {
        state->position++;
        return 1;
    }
    for (;;) {
        int read = container->kind == WEIVEL_JSON_OBJECT ? read_object_member(state, container)
                                                  : read_array_element(state, container);
        if (!read) {
            return 0;
        }
        skip_whitespace(state);
        if (peek(state) == closing) {
            state->position++;
            return 1;
        }
        if (peek(state) != ',') {
            fail(state, container->kind == WEIVEL_JSON_OBJECT ? "expected ',' or '}'" : "expected ',' or ']'");
            return 0;
        }
        state->position++;
        skip_whitespace(state);
        if (peek(state) == closing) {
            fail(state, "trailing comma");
            return 0;
        }
    }
}

static weivel_json_value *parse_container(parser *state, weivel_json_kind kind, char closing) {
    weivel_json_value *container;
    if (!enter_container(state)) {
        return NULL;
    }
    container = new_value(kind);
    if (container == NULL) {
        return fail_out_of_memory(state);
    }
    state->position++;
    if (!read_container_items(state, container, closing)) {
        weivel_json_free(container);
        return NULL;
    }
    state->depth--;
    return container;
}

static weivel_json_value *parse_value(parser *state) {
    skip_whitespace(state);
    if (!has_more(state)) {
        return fail(state, "unexpected end of input");
    }
    switch (peek(state)) {
        case '{': return parse_container(state, WEIVEL_JSON_OBJECT, '}');
        case '[': return parse_container(state, WEIVEL_JSON_ARRAY, ']');
        case '"': return parse_string(state);
        case 't': return parse_literal(state, "true", WEIVEL_JSON_BOOL, 1);
        case 'f': return parse_literal(state, "false", WEIVEL_JSON_BOOL, 0);
        case 'n': return parse_literal(state, "null", WEIVEL_JSON_NULL, 0);
        default: return is_number_start(peek(state)) ? parse_number(state) : fail(state, "unexpected character");
    }
}

static int is_continuation(const char *text, size_t length, size_t index) {
    return index < length && ((unsigned char)text[index] & 0xC0) == 0x80;
}

/**
 * Returns how many bytes the well-formed UTF-8 sequence at index spans, or 0 when it is not
 * well-formed. Follows the table in RFC 3629: the second byte of E0, ED, F0 and F4 is narrowed so
 * overlong forms, surrogates and anything past U+10FFFF are refused.
 */
static size_t utf8_sequence_length(const char *text, size_t length, size_t index) {
    unsigned char lead = (unsigned char)text[index];
    unsigned char second_minimum = 0x80;
    unsigned char second_maximum = 0xBF;
    size_t span;
    size_t position;
    if (lead < 0x80) {
        return 1;
    }
    if (lead >= 0xC2 && lead <= 0xDF) {
        span = 2;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        span = 3;
        second_minimum = lead == 0xE0 ? 0xA0 : 0x80;
        second_maximum = lead == 0xED ? 0x9F : 0xBF;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        span = 4;
        second_minimum = lead == 0xF0 ? 0x90 : 0x80;
        second_maximum = lead == 0xF4 ? 0x8F : 0xBF;
    } else {
        return 0;
    }
    if (index + 1 >= length || (unsigned char)text[index + 1] < second_minimum ||
        (unsigned char)text[index + 1] > second_maximum) {
        return 0;
    }
    for (position = index + 2; position < index + span; position++) {
        if (!is_continuation(text, length, position)) {
            return 0;
        }
    }
    return span;
}

/** Returns the offset of the first byte that starts ill-formed UTF-8, or length when all of it is well-formed. */
static size_t first_invalid_utf8(const char *text, size_t length) {
    size_t index = 0;
    while (index < length) {
        size_t span = utf8_sequence_length(text, length, index);
        if (span == 0) {
            return index;
        }
        index += span;
    }
    return length;
}

static int starts_with_byte_order_mark(const char *text, size_t length) {
    return length >= 3 && memcmp(text, "\xEF\xBB\xBF", 3) == 0;
}

static int refuse_unreadable_text(parser *state) {
    size_t invalid;
    if (starts_with_byte_order_mark(state->text, state->length)) {
        fail(state, "byte order mark at the start of the document");
        return 1;
    }
    invalid = first_invalid_utf8(state->text, state->length);
    if (invalid < state->length) {
        state->position = invalid;
        fail(state, "invalid UTF-8");
        return 1;
    }
    return 0;
}

static void report_failure(const parser *state, weivel_json_error *error) {
    *error = state->failure;
    if (state->failure.message == state->failure.detail) {
        error->message = error->detail;
    }
}

weivel_json_value *weivel_json_parse(const char *text, size_t length, weivel_json_error *error) {
    parser state;
    weivel_json_value *root = NULL;
    memset(&state, 0, sizeof(state));
    state.text = text;
    state.length = length;
    if (!refuse_unreadable_text(&state)) {
        root = parse_value(&state);
    }
    if (root != NULL) {
        skip_whitespace(&state);
        if (has_more(&state)) {
            weivel_json_free(root);
            root = fail(&state, "unexpected text after the document");
        }
    }
    if (root == NULL && error != NULL) {
        report_failure(&state, error);
    }
    return root;
}

void weivel_json_free(weivel_json_value *value) {
    size_t index;
    if (value == NULL) {
        return;
    }
    for (index = 0; index < value->count; index++) {
        weivel_standard_memory_release(value->members[index].key);
        weivel_json_free(value->members[index].value);
    }
    weivel_standard_memory_release(value->members);
    weivel_standard_memory_release(value->text);
    weivel_standard_memory_release(value);
}

weivel_json_kind weivel_json_kind_of(const weivel_json_value *value) {
    return value == NULL ? WEIVEL_JSON_NULL : value->kind;
}

const weivel_json_value *weivel_json_object_get(const weivel_json_value *value, const char *key) {
    size_t index;
    if (value == NULL || value->kind != WEIVEL_JSON_OBJECT) {
        return NULL;
    }
    for (index = 0; index < value->count; index++) {
        if (strcmp(value->members[index].key, key) == 0) {
            return value->members[index].value;
        }
    }
    return NULL;
}

size_t weivel_json_object_size(const weivel_json_value *value) {
    return value != NULL && value->kind == WEIVEL_JSON_OBJECT ? value->count : 0;
}

const char *weivel_json_object_key_at(const weivel_json_value *value, size_t index) {
    return index < weivel_json_object_size(value) ? value->members[index].key : NULL;
}

const weivel_json_value *weivel_json_object_value_at(const weivel_json_value *value, size_t index) {
    return index < weivel_json_object_size(value) ? value->members[index].value : NULL;
}

size_t weivel_json_array_length(const weivel_json_value *value) {
    return value != NULL && value->kind == WEIVEL_JSON_ARRAY ? value->count : 0;
}

const weivel_json_value *weivel_json_array_at(const weivel_json_value *value, size_t index) {
    return index < weivel_json_array_length(value) ? value->members[index].value : NULL;
}

const char *weivel_json_as_string(const weivel_json_value *value) {
    return value != NULL && value->kind == WEIVEL_JSON_STRING ? value->text : NULL;
}

size_t weivel_json_string_length(const weivel_json_value *value) {
    return value != NULL && value->kind == WEIVEL_JSON_STRING ? value->text_length : 0;
}

int weivel_json_as_bool(const weivel_json_value *value, int *out) {
    if (value == NULL || value->kind != WEIVEL_JSON_BOOL) {
        return 0;
    }
    *out = value->boolean;
    return 1;
}

int weivel_json_as_int64(const weivel_json_value *value, int64_t *out) {
    if (value == NULL || value->kind != WEIVEL_JSON_NUMBER || !value->is_integer) {
        return 0;
    }
    *out = value->integer;
    return 1;
}

int weivel_json_as_double(const weivel_json_value *value, double *out) {
    if (value == NULL || value->kind != WEIVEL_JSON_NUMBER) {
        return 0;
    }
    *out = value->real;
    return 1;
}
