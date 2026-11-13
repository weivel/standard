#include "weivel/identity.h"

#include "allocation.h"
#include "patterns.h"
#include "report.h"

#include <string.h>

#define LOCAL_PREFIX "local:"

static char *copy_bytes(const char *text, size_t length) {
    char *copy = weivel_standard_memory_allocate(length + 1);
    if (copy != NULL) {
        memcpy(copy, text, length);
        copy[length] = '\0';
    }
    return copy;
}

static size_t length_until_slash(const char *text, size_t length, size_t from) {
    size_t at = from;
    while (at < length && text[at] != '/') {
        at++;
    }
    return at - from;
}

static int is_lowercase_letter(char byte) {
    return byte >= 'a' && byte <= 'z';
}

static int is_digit(char byte) {
    return byte >= '0' && byte <= '9';
}

static size_t skip_while(const char *text, size_t length, size_t from, int (*accepts)(char byte)) {
    while (from < length && accepts(text[from])) {
        from++;
    }
    return from;
}

static int is_forge_character(char byte) {
    return is_lowercase_letter(byte) || is_digit(byte) || byte == '-';
}

static int is_host_character(char byte) {
    return is_forge_character(byte) || byte == '.';
}

static int is_segment_character(char byte) {
    return is_host_character(byte) || byte == '_';
}

static int is_dot_segment(const char *text, size_t length) {
    return (length == 1 && text[0] == '.') || (length == 2 && text[0] == '.' && text[1] == '.');
}

/** The offset where the path segments, which start at `from`, stop matching. */
static size_t segments_failure(const char *text, size_t length, size_t from) {
    size_t position = from;
    while (position < length) {
        size_t end;
        if (text[position] != '/') {
            return position;
        }
        end = skip_while(text, length, position + 1, is_segment_character);
        if (end == position + 1 || is_dot_segment(text + position + 1, end - position - 1)) {
            return position + 1;
        }
        position = end;
    }
    return position;
}

static int is_character_start(const char *text, size_t at) {
    return (((unsigned char)text[at]) & 0xC0) != 0x80;
}

/** The offset of the first character a local name may not hold, or where its segments stop matching. */
static size_t local_failure(const char *text, size_t length) {
    size_t prefix = strlen(LOCAL_PREFIX);
    size_t name_end = length_until_slash(text, length, prefix) + prefix;
    size_t previous = prefix;
    size_t at;
    if (name_end == prefix) {
        return prefix;
    }
    for (at = prefix + 1; at <= name_end; at++) {
        if (at < length && !is_character_start(text, at)) {
            continue;
        }
        if (!weivel_standard_matches_repository_id(text, at)) {
            return previous;
        }
        previous = at;
    }
    return segments_failure(text, length, name_end);
}

/** The offset where a forge root `<forge>[@host]:<number>` and its segments stop matching. */
static size_t forge_failure(const char *text, size_t length) {
    size_t position;
    size_t digits_end;
    if (length == 0 || !is_lowercase_letter(text[0])) {
        return 0;
    }
    position = skip_while(text, length, 1, is_forge_character);
    if (position < length && text[position] == '@') {
        size_t host_end = skip_while(text, length, position + 1, is_host_character);
        if (host_end == position + 1) {
            return position + 1;
        }
        position = host_end;
    }
    if (position >= length || text[position] != ':') {
        return position;
    }
    digits_end = skip_while(text, length, position + 1, is_digit);
    return digits_end == position + 1 ? position + 1 : segments_failure(text, length, digits_end);
}

static int is_local(const char *text, size_t length) {
    return length >= strlen(LOCAL_PREFIX) && memcmp(text, LOCAL_PREFIX, strlen(LOCAL_PREFIX)) == 0;
}

static size_t failure_offset(const char *text, size_t length) {
    return is_local(text, length) ? local_failure(text, length) : forge_failure(text, length);
}

static weivel_status refuse(weivel_standard_refusal *refusal, uint32_t code, char *detail, const char *fix) {
    weivel_standard_refusal_texts texts = {NULL, NULL, NULL};
    texts.detail = detail;
    if (refusal == NULL) {
        weivel_standard_release_texts(&texts);
        return WEIVEL_STATUS_REFUSED;
    }
    texts.fix = weivel_standard_format_text("%s", fix);
    texts.path = weivel_standard_format_text("%s", "");
    return weivel_standard_fill_refusal(refusal, code, texts) ? WEIVEL_STATUS_REFUSED
                                                              : WEIVEL_STATUS_OUT_OF_MEMORY;
}

static weivel_status refuse_grammar(weivel_standard_refusal *refusal, const char *text, size_t length) {
    return refuse(refusal, WEIVEL_STANDARD_REFUSAL_INVALID_VALUE,
                  weivel_standard_format_text("byte %zu: not a repository identity", failure_offset(text, length)),
                  "write `local:<name>` or `<forge>[@host]:<number>`, then any path segments");
}

static weivel_status refuse_overflow(weivel_standard_refusal *refusal, size_t digits_at) {
    return refuse(refusal, WEIVEL_STANDARD_REFUSAL_UNREPRESENTABLE,
                  weivel_standard_format_text("byte %zu: the repository number does not fit 64 bits", digits_at),
                  "use a repository number up to 18446744073709551615");
}

static int read_number(const char *text, size_t length, size_t from, size_t *end, uint64_t *number) {
    size_t at = from;
    uint64_t value = 0;
    while (at < length && is_digit(text[at])) {
        uint64_t digit = (uint64_t)(text[at] - '0');
        if (value > (UINT64_MAX - digit) / 10) {
            return 0;
        }
        value = value * 10 + digit;
        at++;
    }
    *end = at;
    *number = value;
    return 1;
}

static size_t count_segments(const char *text, size_t length, size_t from) {
    size_t count = 0;
    size_t at;
    for (at = from; at < length; at++) {
        count += text[at] == '/';
    }
    return count;
}

static int read_segments(const char *text, size_t length, size_t from, weivel_standard_identity *identity) {
    size_t count = count_segments(text, length, from);
    size_t position = from;
    if (count == 0) {
        return 1;
    }
    identity->segments = weivel_standard_memory_allocate_zeroed(count, sizeof *identity->segments);
    if (identity->segments == NULL) {
        return 0;
    }
    while (identity->segment_count < count) {
        size_t segment_length = length_until_slash(text, length, position + 1);
        identity->segments[identity->segment_count] = copy_bytes(text + position + 1, segment_length);
        if (identity->segments[identity->segment_count] == NULL) {
            return 0;
        }
        identity->segment_count++;
        position += 1 + segment_length;
    }
    return 1;
}

static int read_local(const char *text, size_t length, weivel_standard_identity *identity, size_t *root_end) {
    size_t prefix = strlen(LOCAL_PREFIX);
    identity->kind = WEIVEL_STANDARD_IDENTITY_LOCAL;
    identity->name_length = length_until_slash(text, length, prefix);
    *root_end = prefix + identity->name_length;
    identity->name = copy_bytes(text + prefix, identity->name_length);
    return identity->name != NULL;
}

static int read_forge_words(const char *text, size_t length, weivel_standard_identity *identity, size_t *colon) {
    size_t forge_end = skip_while(text, length, 1, is_forge_character);
    identity->kind = WEIVEL_STANDARD_IDENTITY_FORGE;
    identity->forge = copy_bytes(text, forge_end);
    *colon = forge_end;
    if (identity->forge != NULL && text[forge_end] == '@') {
        size_t host_end = skip_while(text, length, forge_end + 1, is_host_character);
        identity->host = copy_bytes(text + forge_end + 1, host_end - forge_end - 1);
        *colon = host_end;
        return identity->host != NULL;
    }
    return identity->forge != NULL;
}

weivel_status weivel_standard_identity_parse(const char *text, size_t length, weivel_standard_identity *identity,
                                             weivel_standard_refusal *refusal) {
    weivel_standard_identity parts;
    size_t root_end;
    if (text == NULL || identity == NULL) {
        return WEIVEL_STATUS_MALFORMED;
    }
    memset(identity, 0, sizeof *identity);
    memset(&parts, 0, sizeof parts);
    if (!weivel_standard_matches_repository_id(text, length)) {
        return refuse_grammar(refusal, text, length);
    }
    if (is_local(text, length)) {
        if (!read_local(text, length, &parts, &root_end)) {
            goto out_of_memory;
        }
    } else {
        size_t colon;
        if (!read_forge_words(text, length, &parts, &colon)) {
            goto out_of_memory;
        }
        if (!read_number(text, length, colon + 1, &root_end, &parts.number)) {
            weivel_standard_identity_release(&parts);
            return refuse_overflow(refusal, colon + 1);
        }
    }
    if (!read_segments(text, length, root_end, &parts)) {
        goto out_of_memory;
    }
    *identity = parts;
    return WEIVEL_STATUS_OK;
out_of_memory:
    weivel_standard_identity_release(&parts);
    return WEIVEL_STATUS_OUT_OF_MEMORY;
}

void weivel_standard_identity_release(weivel_standard_identity *identity) {
    size_t at;
    if (identity == NULL) {
        return;
    }
    for (at = 0; at < identity->segment_count; at++) {
        weivel_standard_memory_release(identity->segments[at]);
    }
    weivel_standard_memory_release(identity->segments);
    weivel_standard_memory_release(identity->name);
    weivel_standard_memory_release(identity->forge);
    weivel_standard_memory_release(identity->host);
    memset(identity, 0, sizeof *identity);
}

static int order_of(int difference) {
    return difference < 0 ? -1 : difference > 0 ? 1 : 0;
}

static int compare_names(const weivel_standard_identity *left, const weivel_standard_identity *right) {
    size_t shared = left->name_length < right->name_length ? left->name_length : right->name_length;
    int difference = memcmp(left->name, right->name, shared);
    if (difference != 0) {
        return order_of(difference);
    }
    return left->name_length < right->name_length ? -1 : left->name_length > right->name_length ? 1 : 0;
}

static int compare_hosts(const char *left, const char *right) {
    if (left == NULL || right == NULL) {
        return (left != NULL) - (right != NULL);
    }
    return order_of(strcmp(left, right));
}

static int compare_forge_roots(const weivel_standard_identity *left, const weivel_standard_identity *right) {
    int difference = order_of(strcmp(left->forge, right->forge));
    if (difference == 0) {
        difference = compare_hosts(left->host, right->host);
    }
    if (difference == 0) {
        difference = left->number < right->number ? -1 : left->number > right->number ? 1 : 0;
    }
    return difference;
}

static int compare_segments(const weivel_standard_identity *left, const weivel_standard_identity *right) {
    size_t at;
    for (at = 0; at < left->segment_count && at < right->segment_count; at++) {
        int difference = order_of(strcmp(left->segments[at], right->segments[at]));
        if (difference != 0) {
            return difference;
        }
    }
    return left->segment_count < right->segment_count ? -1 : left->segment_count > right->segment_count ? 1 : 0;
}

int weivel_standard_identity_compare(const weivel_standard_identity *left, const weivel_standard_identity *right) {
    int difference;
    if (left->kind != right->kind) {
        return left->kind < right->kind ? -1 : 1;
    }
    difference = left->kind == WEIVEL_STANDARD_IDENTITY_LOCAL ? compare_names(left, right)
                                                              : compare_forge_roots(left, right);
    return difference != 0 ? difference : compare_segments(left, right);
}
