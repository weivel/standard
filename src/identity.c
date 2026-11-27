#include "weivel/identity.h"

#include "allocation.h"
#include "patterns.h"
#include "report.h"
#include "sized.h"

#include <string.h>

#define LOCAL_PREFIX "local:"
#define GIT_PREFIX "git:"
#define SHORT_OBJECT_ID 40u
#define LONG_OBJECT_ID 64u

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

static int is_digit(char byte) {
    return byte >= '0' && byte <= '9';
}

static int is_lowercase_hex(char byte) {
    return is_digit(byte) || (byte >= 'a' && byte <= 'f');
}

static int is_segment_character(char byte) {
    return (byte >= 'a' && byte <= 'z') || is_digit(byte) || byte == '.' || byte == '_' || byte == '-';
}

static int is_dot_segment(const char *text, size_t length) {
    return (length == 1 && text[0] == '.') || (length == 2 && text[0] == '.' && text[1] == '.');
}

static int starts_with(const char *text, size_t length, const char *prefix) {
    return length >= strlen(prefix) && memcmp(text, prefix, strlen(prefix)) == 0;
}

/** The offset where the path segments, which start at `from`, stop matching. */
static size_t segments_failure(const char *text, size_t length, size_t from) {
    size_t position = from;
    while (position < length) {
        size_t begin = position + 1;
        size_t end = begin + length_until_slash(text, length, begin);
        size_t at;
        if (end == begin || is_dot_segment(text + begin, end - begin)) {
            return begin;
        }
        for (at = begin; at < end; at++) {
            if (!is_segment_character(text[at])) {
                return at;
            }
        }
        position = end;
    }
    return position;
}

/** The offset of the first character a local name may not hold, or where its segments stop matching. */
static size_t local_failure(const char *text, size_t length) {
    size_t name_end = weivel_standard_local_name_end(text, length);
    if (name_end == strlen(LOCAL_PREFIX) || (name_end < length && text[name_end] != '/')) {
        return name_end;
    }
    return segments_failure(text, length, name_end);
}

static size_t hex_run_length(const char *text, size_t length, size_t from) {
    size_t at = from;
    while (at < length && is_lowercase_hex(text[at])) {
        at++;
    }
    return at - from;
}

/** The digits of a run that still belong to an object id: a run past 40 or 64 stops being one at the digit after. */
static size_t object_id_digits(size_t run) {
    return run >= LONG_OBJECT_ID ? LONG_OBJECT_ID : run >= SHORT_OBJECT_ID ? SHORT_OBJECT_ID : run;
}

/** The offset where a git root `git:<40 or 64 lowercase hex>` and its segments stop matching. */
static size_t git_failure(const char *text, size_t length) {
    size_t prefix = strlen(GIT_PREFIX);
    size_t run = hex_run_length(text, length, prefix);
    size_t root_end = prefix + run;
    if (run != SHORT_OBJECT_ID && run != LONG_OBJECT_ID) {
        return prefix + object_id_digits(run);
    }
    if (root_end < length && text[root_end] != '/') {
        return root_end;
    }
    return segments_failure(text, length, root_end);
}

static size_t failure_offset(const char *text, size_t length) {
    if (starts_with(text, length, LOCAL_PREFIX)) {
        return local_failure(text, length);
    }
    return starts_with(text, length, GIT_PREFIX) ? git_failure(text, length) : 0;
}

static weivel_status refuse_grammar(weivel_standard_refusal *refusal, const char *text, size_t length) {
    weivel_standard_refusal_texts texts = {NULL, NULL, NULL};
    if (refusal == NULL) {
        return WEIVEL_STATUS_REFUSED;
    }
    texts.detail =
        weivel_standard_format_text("byte %zu: not a repository identity", failure_offset(text, length));
    texts.fix = weivel_standard_format_text(
        "%s", "write `local:<name>` or `git:<the root commit's 40 or 64 lowercase hex digits>`, then any path segments");
    texts.path = weivel_standard_format_text("%s", "");
    return weivel_standard_fill_refusal(refusal, WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, texts)
               ? WEIVEL_STATUS_REFUSED
               : WEIVEL_STATUS_OUT_OF_MEMORY;
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

static int read_git(const char *text, size_t length, weivel_standard_identity *identity, size_t *root_end) {
    size_t prefix = strlen(GIT_PREFIX);
    identity->kind = WEIVEL_STANDARD_IDENTITY_GIT;
    identity->commit_length = hex_run_length(text, length, prefix);
    *root_end = prefix + identity->commit_length;
    identity->commit = copy_bytes(text + prefix, identity->commit_length);
    return identity->commit != NULL;
}

static weivel_status refuse_malformed_call(const char *text, const weivel_standard_identity *identity,
                                           weivel_standard_refusal *refusal) {
    if (refusal == NULL) {
        return WEIVEL_STATUS_MALFORMED;
    }
    return text != NULL && identity != NULL ? weivel_standard_refuse_call(refusal,
                                                          "weivel_standard_identity_parse was given an identity "
                                                          "smaller than its first shape",
                                                          "set the identity's size, as WEIVEL_INIT does")
                            : weivel_standard_refuse_call(refusal,
                                                          "weivel_standard_identity_parse was given no text or no "
                                                          "identity to fill",
                                                          "pass the identity's text and the address of a "
                                                          "weivel_standard_identity");
}

weivel_status weivel_standard_identity_parse(const char *text, size_t length, weivel_standard_identity *identity,
                                             weivel_standard_refusal *refusal) {
    weivel_standard_identity parts = WEIVEL_INIT(weivel_standard_identity);
    size_t root_end;
    int read;
    if (weivel_standard_refusal_is_unusable(refusal)) {
        return WEIVEL_STATUS_MALFORMED;
    }
    if (text == NULL || identity == NULL || identity->size < sizeof *identity) {
        return refuse_malformed_call(text, identity, refusal);
    }
    weivel_standard_sized_write(identity, &parts, sizeof parts);
    if (!weivel_standard_matches_repository_id(text, length)) {
        return refuse_grammar(refusal, text, length);
    }
    read = starts_with(text, length, LOCAL_PREFIX) ? read_local(text, length, &parts, &root_end)
                                                   : read_git(text, length, &parts, &root_end);
    if (!read || !read_segments(text, length, root_end, &parts)) {
        weivel_standard_identity_release(&parts);
        return WEIVEL_STATUS_OUT_OF_MEMORY;
    }
    weivel_standard_sized_write(identity, &parts, sizeof parts);
    return WEIVEL_STATUS_OK;
}

void weivel_standard_identity_release(weivel_standard_identity *identity) {
    weivel_standard_identity empty = WEIVEL_INIT(weivel_standard_identity);
    size_t at;
    if (identity == NULL || identity->size < sizeof *identity) {
        return;
    }
    for (at = 0; at < identity->segment_count; at++) {
        weivel_standard_memory_release(identity->segments[at]);
    }
    weivel_standard_memory_release(identity->segments);
    weivel_standard_memory_release(identity->name);
    weivel_standard_memory_release(identity->commit);
    weivel_standard_sized_write(identity, &empty, sizeof empty);
}

static int order_of(int difference) {
    return difference < 0 ? -1 : difference > 0 ? 1 : 0;
}

static int compare_bytes(const char *left, size_t left_length, const char *right, size_t right_length) {
    size_t shared = left_length < right_length ? left_length : right_length;
    int difference = memcmp(left, right, shared);
    if (difference != 0) {
        return order_of(difference);
    }
    return left_length < right_length ? -1 : left_length > right_length ? 1 : 0;
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
    difference = left->kind == WEIVEL_STANDARD_IDENTITY_LOCAL
                     ? compare_bytes(left->name, left->name_length, right->name, right->name_length)
                     : compare_bytes(left->commit, left->commit_length, right->commit, right->commit_length);
    return difference != 0 ? difference : compare_segments(left, right);
}
