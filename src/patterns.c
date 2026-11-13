#include "patterns.h"

#include <string.h>

#define REPLACEMENT_CHARACTER 0xFFFDul

typedef struct character {
    unsigned long code_point;
    size_t width;
} character;

static size_t continuation_count(unsigned char lead) {
    if (lead >= 0xF0 && lead <= 0xF4) {
        return 3;
    }
    if (lead >= 0xE0) {
        return lead <= 0xEF ? 2 : 0;
    }
    return lead >= 0xC2 && lead <= 0xDF ? 1 : 0;
}

/** The character at `position`; a byte that starts no well-formed sequence reads as one replacement character. */
static character character_at(const char *text, size_t length, size_t position) {
    unsigned char lead = (unsigned char)text[position];
    size_t extra = continuation_count(lead);
    character result = {lead, 1};
    size_t at;
    if (lead < 0x80) {
        return result;
    }
    result.code_point = REPLACEMENT_CHARACTER;
    if (extra == 0 || position + extra >= length) {
        return result;
    }
    result.code_point = lead & (0x3Fu >> extra);
    for (at = 1; at <= extra; at++) {
        unsigned char next = (unsigned char)text[position + at];
        if ((next & 0xC0) != 0x80) {
            result.code_point = REPLACEMENT_CHARACTER;
            return result;
        }
        result.code_point = (result.code_point << 6) | (next & 0x3Fu);
    }
    result.width = extra + 1;
    return result;
}

/** ECMAScript's `\s`: its white space and line terminators. */
static int is_ecmascript_space(unsigned long code_point) {
    return (code_point >= 0x09 && code_point <= 0x0D) || code_point == 0x20 || code_point == 0xA0 ||
           code_point == 0x1680 || (code_point >= 0x2000 && code_point <= 0x200A) || code_point == 0x2028 ||
           code_point == 0x2029 || code_point == 0x202F || code_point == 0x205F || code_point == 0x3000 ||
           code_point == 0xFEFF;
}

static int is_control_or_line_separator(unsigned long code_point) {
    return code_point <= 0x1F || (code_point >= 0x7F && code_point <= 0x9F) || code_point == 0x2028 ||
           code_point == 0x2029;
}

static int is_lowercase_letter(char byte) {
    return byte >= 'a' && byte <= 'z';
}

static int is_digit(char byte) {
    return byte >= '0' && byte <= '9';
}

static int is_letter(char byte) {
    return is_lowercase_letter(byte) || (byte >= 'A' && byte <= 'Z');
}

static int is_lowercase_word_character(char byte) {
    return is_lowercase_letter(byte) || is_digit(byte);
}

static int is_separator(char byte) {
    return byte == '/' || byte == '\\';
}

static int equals_text(const char *text, size_t length, const char *literal) {
    return length == strlen(literal) && memcmp(text, literal, length) == 0;
}

static int is_dot_segment(const char *text, size_t length) {
    return equals_text(text, length, ".") || equals_text(text, length, "..");
}

/** The length of the device name `text` starts with: con, prn, aux, nul, or com or lpt and a digit from 1 to 9. */
static size_t reserved_device_prefix(const char *text, size_t length) {
    static const char *const three_letter_names[] = {"con", "prn", "aux", "nul"};
    size_t at;
    for (at = 0; at < sizeof three_letter_names / sizeof three_letter_names[0]; at++) {
        if (length >= 3 && memcmp(text, three_letter_names[at], 3) == 0) {
            return 3;
        }
    }
    if (length >= 4 && (memcmp(text, "com", 3) == 0 || memcmp(text, "lpt", 3) == 0) && text[3] >= '1' &&
        text[3] <= '9') {
        return 4;
    }
    return 0;
}

static int names_a_reserved_device(const char *text, size_t length) {
    size_t prefix = reserved_device_prefix(text, length);
    return prefix > 0 && (prefix == length || text[prefix] == '.');
}

/** `[a-z0-9]+([-.][a-z0-9]+)*` over the whole text. */
static int is_lowercase_words(const char *text, size_t length) {
    size_t at;
    int after_separator = 1;
    for (at = 0; at < length; at++) {
        if (is_lowercase_word_character(text[at])) {
            after_separator = 0;
        } else if ((text[at] == '-' || text[at] == '.') && !after_separator) {
            after_separator = 1;
        } else {
            return 0;
        }
    }
    return length > 0 && !after_separator;
}

int weivel_standard_matches_import_name(const char *text, size_t length) {
    return !names_a_reserved_device(text, length) &&
           is_lowercase_words(text, length);
}

static int holds_control_or_line_separator(const char *text, size_t length) {
    size_t position = 0;
    while (position < length) {
        character next = character_at(text, length, position);
        if (is_control_or_line_separator(next.code_point)) {
            return 1;
        }
        position += next.width;
    }
    return 0;
}

static int holds_parent_segment(const char *text, size_t length) {
    size_t start = 0;
    size_t at;
    for (at = 0; at <= length; at++) {
        if (at == length || is_separator(text[at])) {
            if (equals_text(text + start, at - start, "..")) {
                return 1;
            }
            start = at + 1;
        }
    }
    return 0;
}

/**
 * @implNote The expression's `..` lookahead may begin after any separator, so it refuses a `..` segment anywhere;
 * a segment that only holds dots among other characters (`a..b`, `...`) is not one.
 */
int weivel_standard_matches_entry(const char *text, size_t length) {
    if (length > 0 && is_separator(text[0])) {
        return 0;
    }
    if (length >= 2 && is_letter(text[0]) && text[1] == ':') {
        return 0;
    }
    return !holds_control_or_line_separator(text, length) && !holds_parent_segment(text, length);
}

/** `local:[^/\s]+`: the length of the root it matches, 0 for none. */
static size_t local_root_length(const char *text, size_t length) {
    size_t position = strlen("local:");
    if (length < position || memcmp(text, "local:", position) != 0) {
        return 0;
    }
    while (position < length) {
        character next = character_at(text, length, position);
        if (next.code_point == '/' || is_ecmascript_space(next.code_point)) {
            break;
        }
        position += next.width;
    }
    return position > strlen("local:") ? position : 0;
}

static size_t run_length(const char *text, size_t length, size_t from, int (*accepts)(char byte)) {
    size_t at = from;
    while (at < length && accepts(text[at])) {
        at++;
    }
    return at - from;
}

static int is_forge_character(char byte) {
    return is_lowercase_word_character(byte) || byte == '-';
}

static int is_host_character(char byte) {
    return is_lowercase_word_character(byte) || byte == '.' || byte == '-';
}

/** `[a-z][a-z0-9-]*(@[a-z0-9.-]+)?:[0-9]+`: the length of the root it matches, 0 for none. */
static size_t forge_root_length(const char *text, size_t length) {
    size_t position;
    size_t digits;
    if (length == 0 || !is_lowercase_letter(text[0])) {
        return 0;
    }
    position = 1 + run_length(text, length, 1, is_forge_character);
    if (position < length && text[position] == '@') {
        size_t host = run_length(text, length, position + 1, is_host_character);
        if (host == 0) {
            return 0;
        }
        position += 1 + host;
    }
    if (position >= length || text[position] != ':') {
        return 0;
    }
    digits = run_length(text, length, position + 1, is_digit);
    return digits == 0 ? 0 : position + 1 + digits;
}

static int is_segment_character(char byte) {
    return is_lowercase_word_character(byte) || byte == '.' || byte == '_' || byte == '-';
}

/** `(/(?!\.{1,2}(/|$))[a-z0-9._-]+)*` from `from` to the end. */
static int is_lowercase_segments(const char *text, size_t length, size_t from) {
    size_t position = from;
    while (position < length) {
        size_t segment;
        if (text[position] != '/') {
            return 0;
        }
        segment = run_length(text, length, position + 1, is_segment_character);
        if (segment == 0 || is_dot_segment(text + position + 1, segment)) {
            return 0;
        }
        position += 1 + segment;
    }
    return 1;
}

static int root_then_segments(const char *text, size_t length, size_t root) {
    return root > 0 && is_lowercase_segments(text, length, root);
}

int weivel_standard_matches_repository_id(const char *text, size_t length) {
    return root_then_segments(text, length, local_root_length(text, length)) ||
           root_then_segments(text, length, forge_root_length(text, length));
}

static int is_alphanumeric(char byte) {
    return is_letter(byte) || is_digit(byte);
}

static int is_name_character(char byte) {
    return is_alphanumeric(byte) || byte == '.' || byte == '_' || byte == '-';
}

int weivel_standard_matches_owner_and_name(const char *text, size_t length) {
    size_t owner;
    size_t name;
    if (length == 0 || !is_alphanumeric(text[0])) {
        return 0;
    }
    owner = 1 + run_length(text, length, 1, is_name_character);
    if (owner >= length || text[owner] != '/') {
        return 0;
    }
    name = run_length(text, length, owner + 1, is_name_character);
    return name > 0 && owner + 1 + name == length && !is_dot_segment(text + owner + 1, name);
}
