#include "check.h"
#include "json_equality.h"

static weivel_json_value *parse_text(const char *text) {
    return weivel_json_parse(text, strlen(text), NULL);
}

static int texts_equal(const char *left, const char *right) {
    weivel_json_value *left_value = parse_text(left);
    weivel_json_value *right_value = parse_text(right);
    int equal;
    CHECK(left_value != NULL && right_value != NULL);
    equal = weivel_standard_json_equal(left_value, right_value);
    CHECK(equal == weivel_standard_json_equal(right_value, left_value));
    weivel_json_free(left_value);
    weivel_json_free(right_value);
    return equal;
}

static void compares_values_as_json_schema_does(void) {
    CHECK(texts_equal("1", "1.0"));
    CHECK(texts_equal("1", "1e0"));
    CHECK(texts_equal("[1, \"a\", null, true]", "[1.0, \"a\", null, true]"));
    CHECK(texts_equal("{\"a\": 1, \"b\": [2]}", "{\"b\": [2.0], \"a\": 1}"));
    CHECK(texts_equal("\"a\\u0000b\"", "\"a\\u0000b\""));
    CHECK(!texts_equal("\"a\\u0000b\"", "\"a\\u0000c\""));
    CHECK(!texts_equal("\"a\\u0000b\"", "\"a\""));
    CHECK(!texts_equal("1", "2"));
    CHECK(!texts_equal("1", "\"1\""));
    CHECK(!texts_equal("true", "false"));
    CHECK(!texts_equal("[1, 2]", "[2, 1]"));
    CHECK(!texts_equal("[1]", "[1, 1]"));
    CHECK(!texts_equal("{\"a\": 1}", "{\"a\": 1, \"b\": 1}"));
    CHECK(!texts_equal("{\"a\": 1}", "{\"b\": 1}"));
    CHECK(!texts_equal("{\"a\": 1}", "{\"a\": 2}"));
    CHECK(weivel_standard_json_equal(NULL, NULL));
}

static void compares_large_numbers_without_rounding(void) {
    CHECK(texts_equal("9007199254740992", "9007199254740992.0"));
    CHECK(texts_equal("9007199254740993", "9007199254740993"));
    CHECK(!texts_equal("9007199254740993", "9007199254740992"));
    CHECK(!texts_equal("9007199254740993", "9007199254740992.0"));
    CHECK(!texts_equal("9007199254740993", "9007199254740993.0"));
    CHECK(!texts_equal("-9007199254740993", "-9007199254740992.0"));
    CHECK(!texts_equal("9223372036854775807", "9223372036854775808.0"));
    CHECK(texts_equal("1e300", "1e300"));
    CHECK(texts_equal("1e300", "1.0e300"));
    CHECK(texts_equal("1", "1.0"));
    CHECK(texts_equal("-1", "-1e0"));
    CHECK(!texts_equal("1", "1.5"));
    CHECK(texts_equal("1e15", "1000000000000000.0"));
}

int main(void) {
    compares_values_as_json_schema_does();
    compares_large_numbers_without_rounding();
    return check_summary("json_equality_test");
}
