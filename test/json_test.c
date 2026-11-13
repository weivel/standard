#include "check.h"
#include "weivel/json.h"

#include <locale.h>
#include <stdlib.h>

static weivel_json_value *parse_text(const char *text) {
    return weivel_json_parse(text, strlen(text), NULL);
}

static void check_refused(const char *text, int line, int column, const char *message) {
    weivel_json_error error = {0, 0, NULL};
    weivel_json_value *value = weivel_json_parse(text, strlen(text), &error);
    CHECK(value == NULL);
    check_text(error.message, message, text);
    if (error.line != line || error.column != column) {
        fprintf(stderr, "input %s: expected %d:%d, got %d:%d (%s)\n", text, line, column, error.line, error.column,
                error.message == NULL ? "(null)" : error.message);
    }
    CHECK(error.line == line);
    CHECK(error.column == column);
    weivel_json_free(value);
}

static void reads_objects_and_arrays(void) {
    weivel_json_value *root = parse_text("{\"name\": \"boot\", \"steps\": [1, 2, 3], \"nested\": {\"empty\": []}}");
    const weivel_json_value *steps;
    int64_t second = 0;
    CHECK(root != NULL);
    CHECK(weivel_json_kind_of(root) == WEIVEL_JSON_OBJECT);
    CHECK(weivel_json_object_size(root) == 3);
    check_text(weivel_json_object_key_at(root, 1), "steps", "key at 1");
    check_text(weivel_json_as_string(weivel_json_object_get(root, "name")), "boot", "name");
    CHECK(weivel_json_object_get(root, "absent") == NULL);
    steps = weivel_json_object_get(root, "steps");
    CHECK(weivel_json_array_length(steps) == 3);
    CHECK(weivel_json_as_int64(weivel_json_array_at(steps, 1), &second) && second == 2);
    CHECK(weivel_json_array_at(steps, 3) == NULL);
    CHECK(weivel_json_array_length(weivel_json_object_get(weivel_json_object_get(root, "nested"), "empty")) == 0);
    CHECK(weivel_json_object_value_at(root, 0) == weivel_json_object_get(root, "name"));
    weivel_json_free(root);
}

static void reads_strings_with_escapes(void) {
    weivel_json_value *root = parse_text("[\"a\\n\\t\\\"\\\\\\/\", \"\\u00e9\", \"\\ud83d\\ude00\", \"a\\u0000b\"]");
    CHECK(root != NULL);
    check_text(weivel_json_as_string(weivel_json_array_at(root, 0)), "a\n\t\"\\/", "simple escapes");
    check_text(weivel_json_as_string(weivel_json_array_at(root, 1)), "\xC3\xA9", "two-byte escape");
    check_text(weivel_json_as_string(weivel_json_array_at(root, 2)), "\xF0\x9F\x98\x80", "surrogate pair");
    CHECK(weivel_json_string_length(weivel_json_array_at(root, 3)) == 3);
    weivel_json_free(root);
}

static void reads_numbers_as_integer_and_double(void) {
    weivel_json_value *root = parse_text("[0, -7, 9223372036854775807, -9223372036854775808, 9223372036854775808, 1.5, 2e3]");
    int64_t integer = 0;
    double real = 0;
    CHECK(root != NULL);
    CHECK(weivel_json_as_int64(weivel_json_array_at(root, 0), &integer) && integer == 0);
    CHECK(weivel_json_as_int64(weivel_json_array_at(root, 1), &integer) && integer == -7);
    CHECK(weivel_json_as_int64(weivel_json_array_at(root, 2), &integer) && integer == INT64_MAX);
    CHECK(weivel_json_as_int64(weivel_json_array_at(root, 3), &integer) && integer == INT64_MIN);
    CHECK(!weivel_json_as_int64(weivel_json_array_at(root, 4), &integer));
    CHECK(weivel_json_as_double(weivel_json_array_at(root, 4), &real) && real > 9.2e18);
    CHECK(!weivel_json_as_int64(weivel_json_array_at(root, 5), &integer));
    CHECK(weivel_json_as_double(weivel_json_array_at(root, 5), &real) && real == 1.5);
    CHECK(!weivel_json_as_int64(weivel_json_array_at(root, 6), &integer));
    CHECK(weivel_json_as_double(weivel_json_array_at(root, 6), &real) && real == 2000.0);
    CHECK(!weivel_json_as_int64(NULL, &integer));
    weivel_json_free(root);
}

static void reads_literals(void) {
    weivel_json_value *root = parse_text(" [true, false, null] ");
    int flag = -1;
    CHECK(root != NULL);
    CHECK(weivel_json_as_bool(weivel_json_array_at(root, 0), &flag) && flag == 1);
    CHECK(weivel_json_as_bool(weivel_json_array_at(root, 1), &flag) && flag == 0);
    CHECK(weivel_json_kind_of(weivel_json_array_at(root, 2)) == WEIVEL_JSON_NULL);
    CHECK(!weivel_json_as_bool(weivel_json_array_at(root, 2), &flag));
    weivel_json_free(root);
}

static void refuses_a_trailing_comma(void) {
    check_refused("[1,\n 2,\n]", 3, 1, "trailing comma");
    check_refused("{\"a\": 1,}", 1, 9, "trailing comma");
}

static void refuses_an_unterminated_string(void) {
    check_refused("[\"abc", 1, 6, "unterminated string");
    check_refused("{\"a\":\n\"b", 2, 3, "unterminated string");
}

static void refuses_a_lone_surrogate(void) {
    check_refused("[\"\\ud83d\"]", 1, 9, "lone high surrogate");
    check_refused("[\"\\ude00\"]", 1, 9, "lone low surrogate");
    check_refused("[\"\\ud83d\\u0041\"]", 1, 15,
                  "high surrogate not followed by a low surrogate");
}

static void refuses_other_malformed_input(void) {
    check_refused("", 1, 1, "unexpected end of input");
    check_refused("[1] x", 1, 5, "unexpected text after the document");
    check_refused("[01]", 1, 3, "leading zero in number");
    check_refused("{a: 1}", 1, 2, "expected a string key");
    check_refused("[\"a\tb\"]", 1, 4, "control character in string");
}

static void refuses_a_duplicate_key(void) {
    check_refused("{\"a\": 1,\n \"a\": 2}", 2, 2, "duplicate key \"a\"");
    check_refused("{\"x\": {\"b\":1,\"b\":2}}", 1, 14, "duplicate key \"b\"");
    check_refused("{\"a\\u0062\": 1, \"ab\": 2}", 1, 16, "duplicate key \"ab\"");
}

static void accepts_the_same_key_in_different_objects(void) {
    weivel_json_value *root = parse_text("[{\"a\": 1}, {\"a\": 2}, {\"a\": {\"a\": 3}}]");
    CHECK(root != NULL);
    weivel_json_free(root);
}

static void refuses_a_number_that_is_not_finite(void) {
    check_refused("[1e999]", 1, 2, "number is not finite");
    check_refused("[-1e999]", 1, 2, "number is not finite");
    check_refused("{\"a\": 1E400}", 1, 7, "number is not finite");
}

static void reads_a_number_that_underflows_to_zero(void) {
    weivel_json_value *root = parse_text("[1e-999]");
    double real = 1;
    CHECK(root != NULL);
    CHECK(weivel_json_as_double(weivel_json_array_at(root, 0), &real) && real == 0.0);
    weivel_json_free(root);
}

static void refuses_text_that_is_not_utf8(void) {
    check_refused("[\"\xC0\x80\"]", 1, 3, "invalid UTF-8");
    check_refused("[\"\xE0\x80\x80\"]", 1, 3, "invalid UTF-8");
    check_refused("[\"\xF0\x80\x80\x80\"]", 1, 3, "invalid UTF-8");
    check_refused("[\"\xED\xA0\x80\"]", 1, 3, "invalid UTF-8");
    check_refused("[\"\xF4\x90\x80\x80\"]", 1, 3, "invalid UTF-8");
    check_refused("[\"\xF5\x80\x80\x80\"]", 1, 3, "invalid UTF-8");
    check_refused("[\"\x80\"]", 1, 3, "invalid UTF-8");
    check_refused("[\"\xE2\x82\"]", 1, 3, "invalid UTF-8");
    check_refused("[\"\xFF\"]", 1, 3, "invalid UTF-8");
    check_refused("[\"a\xE2\x82", 1, 4, "invalid UTF-8");
}

static void reads_the_edges_of_valid_utf8(void) {
    weivel_json_value *root = parse_text("[\"\xC2\x80\", \"\xDF\xBF\", \"\xED\x9F\xBF\", \"\xEF\xBF\xBF\", \"\xF0\x90\x80\x80\", \"\xF4\x8F\xBF\xBF\"]");
    CHECK(root != NULL);
    CHECK(weivel_json_array_length(root) == 6);
    weivel_json_free(root);
}

static void refuses_a_byte_order_mark(void) {
    check_refused("\xEF""\xBB""\xBF""{}", 1, 1, "byte order mark at the start of the document");
}

static void names_a_stray_character(void) {
    check_refused("@", 1, 1, "unexpected character");
    check_refused("[1, @]", 1, 5, "unexpected character");
    check_refused("[tru]", 1, 2, "unexpected character");
    check_refused("[-]", 1, 3, "expected a digit");
}

static void cuts_a_long_duplicate_key_on_a_character_boundary(void) {
    char key[64];
    char text[200];
    char expected[100];
    memset(key, 'a', 59);
    key[59] = '\xC3';
    key[60] = '\xA9';
    key[61] = '\0';
    snprintf(text, sizeof(text), "{\"%s\":1,\"%s\":2}", key, key);
    snprintf(expected, sizeof(expected), "duplicate key \"%.59s\"", key);
    check_refused(text, 1, 68, expected);
}

static void refuses_a_nul_in_an_object_key(void) {
    check_refused("{\"a\\u0000b\": 1}", 1, 2, "object key contains a NUL character");
    check_refused("{\"x\": 1, \"\\u0000\": 2}", 1, 10, "object key contains a NUL character");
}

static void keeps_the_length_of_a_string_value_with_a_nul(void) {
    weivel_json_value *root = parse_text("{\"k\": \"a\\u0000b\"}");
    CHECK(root != NULL);
    CHECK(weivel_json_string_length(weivel_json_object_get(root, "k")) == 3);
    weivel_json_free(root);
}

static int selects_a_decimal_comma_locale(void) {
    static const char *const candidates[] = {"de_DE.UTF-8", "de_DE", "fr_FR.UTF-8", "de-DE", "German_Germany", "fr-FR"};
    size_t index;
    for (index = 0; index < sizeof(candidates) / sizeof(candidates[0]); index++) {
        if (setlocale(LC_NUMERIC, candidates[index]) != NULL && localeconv()->decimal_point[0] == ',') {
            return 1;
        }
    }
    return 0;
}

static void reads_numbers_the_same_in_a_decimal_comma_locale(void) {
    weivel_json_value *root;
    double real = 0;
    if (!selects_a_decimal_comma_locale()) {
        setlocale(LC_NUMERIC, "C");
        fprintf(stderr, "skipped: no decimal comma locale is installed on this machine\n");
        return;
    }
    root = parse_text("[1.5, -2.25e1]");
    setlocale(LC_NUMERIC, "C");
    CHECK(root != NULL);
    CHECK(weivel_json_as_double(weivel_json_array_at(root, 0), &real) && real == 1.5);
    CHECK(weivel_json_as_double(weivel_json_array_at(root, 1), &real) && real == -22.5);
    weivel_json_free(root);
}

static void refuses_nesting_that_would_overflow_the_stack(void) {
    size_t depth = 1000;
    char *text = malloc(depth + 1);
    weivel_json_error error = {0, 0, NULL};
    CHECK(text != NULL);
    memset(text, '[', depth);
    text[depth] = '\0';
    CHECK(weivel_json_parse(text, depth, &error) == NULL);
    check_text(error.message, "nesting too deep", "depth message");
    free(text);
}

static char *build_large_document(size_t *length) {
    size_t capacity = 80 * 1024;
    char *text = malloc(capacity);
    size_t used = 0;
    size_t index;
    CHECK(text != NULL);
    used += (size_t)snprintf(text + used, capacity - used, "{\"items\": [");
    for (index = 0; index < 2000; index++) {
        used += (size_t)snprintf(text + used, capacity - used, "%s{\"id\": %u, \"label\": \"entry-%u\"}", index == 0 ? "" : ", ",
                                (unsigned)index, (unsigned)index);
    }
    used += (size_t)snprintf(text + used, capacity - used, "]}");
    *length = used;
    return text;
}

static void reads_a_64_kilobyte_vector(void) {
    size_t length = 0;
    char *text = build_large_document(&length);
    weivel_json_value *root;
    const weivel_json_value *items;
    int64_t id = 0;
    CHECK(length > 64 * 1024);
    root = weivel_json_parse(text, length, NULL);
    CHECK(root != NULL);
    items = weivel_json_object_get(root, "items");
    CHECK(weivel_json_array_length(items) == 2000);
    CHECK(weivel_json_as_int64(weivel_json_object_get(weivel_json_array_at(items, 1999), "id"), &id) && id == 1999);
    check_text(weivel_json_as_string(weivel_json_object_get(weivel_json_array_at(items, 1234), "label")), "entry-1234", "label");
    weivel_json_free(root);
    free(text);
}

static void reads_a_missing_value_as_null(void) {
    CHECK(weivel_json_kind_of(NULL) == WEIVEL_JSON_NULL);
}

int main(void) {
    reads_objects_and_arrays();
    reads_strings_with_escapes();
    reads_numbers_as_integer_and_double();
    reads_literals();
    refuses_a_trailing_comma();
    refuses_an_unterminated_string();
    refuses_a_lone_surrogate();
    refuses_other_malformed_input();
    refuses_a_duplicate_key();
    accepts_the_same_key_in_different_objects();
    refuses_a_number_that_is_not_finite();
    reads_a_number_that_underflows_to_zero();
    refuses_text_that_is_not_utf8();
    reads_the_edges_of_valid_utf8();
    refuses_a_byte_order_mark();
    names_a_stray_character();
    cuts_a_long_duplicate_key_on_a_character_boundary();
    refuses_a_nul_in_an_object_key();
    keeps_the_length_of_a_string_value_with_a_nul();
    reads_numbers_the_same_in_a_decimal_comma_locale();
    refuses_nesting_that_would_overflow_the_stack();
    reads_a_64_kilobyte_vector();
    reads_a_missing_value_as_null();
    return check_summary("json_test");
}
