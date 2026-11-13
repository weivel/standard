#include "check.h"
#include "weivel/mirrors.h"

#include <stdio.h>
#include <stdlib.h>

#define HASH "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"

#ifndef WEIVEL_STANDARD_MIRRORS_FIXTURE
#error "the build names the scratch file"
#endif

/** Single quotes stand for double quotes, so the documents read as JSON on the page. */
static const char *json_of(const char *quoted) {
    static char text[2048];
    size_t at;
    for (at = 0; quoted[at] != '\0' && at + 1 < sizeof text; at++) {
        text[at] = quoted[at] == '\'' ? '"' : quoted[at];
    }
    text[at] = '\0';
    return text;
}

static void reads_an_ordered_list(void) {
    weivel_standard_mirror_list list = {0};
    weivel_standard_refusal refusal = {0};
    const char *json = json_of("{'mirrors': ['https://example.org/core/{sha256}', 'file:///srv/core/{sha256}',"
                               " 'https://example.com/{sha256}.bin'], 'later': {'field': 1}}");
    CHECK(weivel_standard_read_mirrors(json, strlen(json), &list, &refusal) == WEIVEL_STATUS_OK);
    CHECK(list.count == 3);
    if (list.count == 3) {
        check_text(list.templates[0], "https://example.org/core/{sha256}", "first");
        check_text(list.templates[1], "file:///srv/core/{sha256}", "second");
        check_text(list.templates[2], "https://example.com/{sha256}.bin", "third");
    }
    weivel_standard_mirror_list_release(&list);
    CHECK(list.templates == NULL && list.count == 0 && list.storage == NULL);
    weivel_standard_mirror_list_release(&list);
    weivel_standard_mirror_list_release(NULL);
}

typedef struct refused_case {
    const char *name;
    const char *json;
    uint32_t code;
    const char *path;
} refused_case;

static const refused_case REFUSED[] = {
    {"not json", "{'mirrors': ", WEIVEL_STANDARD_REFUSAL_MALFORMED_JSON, ""},
    {"document not an object", "['https://example.org/{sha256}']", WEIVEL_STANDARD_REFUSAL_WRONG_TYPE, ""},
    {"mirrors absent", "{'other': []}", WEIVEL_STANDARD_REFUSAL_MISSING_FIELD, "mirrors"},
    {"mirrors an object", "{'mirrors': {}}", WEIVEL_STANDARD_REFUSAL_WRONG_TYPE, "mirrors"},
    {"mirrors a string", "{'mirrors': 'https://example.org/{sha256}'}", WEIVEL_STANDARD_REFUSAL_WRONG_TYPE,
     "mirrors"},
    {"empty list", "{'mirrors': []}", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "mirrors"},
    {"entry not a string", "{'mirrors': ['https://example.org/{sha256}', 7]}", WEIVEL_STANDARD_REFUSAL_WRONG_TYPE,
     "mirrors[1]"},
    {"entry null", "{'mirrors': [null]}", WEIVEL_STANDARD_REFUSAL_WRONG_TYPE, "mirrors[0]"},
    {"no placeholder", "{'mirrors': ['https://example.org/core']}", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE,
     "mirrors[0]"},
    {"two placeholders", "{'mirrors': ['https://example.org/{sha256}/{sha256}']}",
     WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "mirrors[0]"},
    {"empty entry", "{'mirrors': ['https://example.org/{sha256}', '']}", WEIVEL_STANDARD_REFUSAL_INVALID_VALUE,
     "mirrors[1]"},
    {"nul in entry", "{'mirrors': ['https://example.org/{sha256}\\u0000']}", WEIVEL_STANDARD_REFUSAL_UNREPRESENTABLE,
     "mirrors[0]"}};

static void refuses_a_malformed_list(void) {
    size_t at;
    for (at = 0; at < sizeof REFUSED / sizeof REFUSED[0]; at++) {
        weivel_standard_mirror_list list = {0};
        weivel_standard_refusal refusal = {0};
        const char *json = json_of(REFUSED[at].json);
        CHECK(weivel_standard_read_mirrors(json, strlen(json), &list, &refusal) == WEIVEL_STATUS_REFUSED);
        if (refusal.code != REFUSED[at].code) {
            fprintf(stderr, "%s: code %u, expected %u\n", REFUSED[at].name, (unsigned)refusal.code,
                    (unsigned)REFUSED[at].code);
            check_failures++;
        }
        check_text(refusal.path, REFUSED[at].path, REFUSED[at].name);
        CHECK(refusal.detail != NULL && refusal.fix != NULL);
        CHECK(list.templates == NULL && list.count == 0 && list.storage == NULL);
        weivel_standard_refusal_release(&refusal);
    }
}

static void refuses_a_malformed_call(void) {
    weivel_standard_mirror_list list = {0};
    weivel_standard_refusal refusal = {0};
    const char *json = json_of("{'mirrors': ['https://example.org/{sha256}']}");
    CHECK(weivel_standard_read_mirrors(NULL, 0, &list, &refusal) == WEIVEL_STATUS_MALFORMED);
    CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_MALFORMED_CALL);
    weivel_standard_refusal_release(&refusal);
    CHECK(weivel_standard_read_mirrors(json, strlen(json), NULL, &refusal) == WEIVEL_STATUS_MALFORMED);
    weivel_standard_refusal_release(&refusal);
    CHECK(weivel_standard_read_mirrors(json, strlen(json), &list, NULL) == WEIVEL_STATUS_OK);
    CHECK(weivel_standard_read_mirrors(json, strlen(json), &list, &refusal) == WEIVEL_STATUS_MALFORMED);
    CHECK(list.count == 1);
    weivel_standard_refusal_release(&refusal);
    weivel_standard_mirror_list_release(&list);
}

static void expands_a_template(void) {
    char *url = NULL;
    weivel_standard_refusal refusal = {0};
    CHECK(weivel_standard_mirror_expand("https://example.org/core/{sha256}", HASH, &url, &refusal) ==
          WEIVEL_STATUS_OK);
    check_text(url, "https://example.org/core/" HASH, "expanded");
    weivel_standard_text_release(url);
    url = NULL;
    CHECK(weivel_standard_mirror_expand("{sha256}", HASH, &url, NULL) == WEIVEL_STATUS_OK);
    check_text(url, HASH, "only the placeholder");
    weivel_standard_text_release(url);
    weivel_standard_text_release(NULL);
}

static void refuses_a_bad_hash(void) {
    const char *bad_hashes[] = {"", "abc", "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD",
                                "../../etc/passwd", HASH "0"};
    size_t at;
    for (at = 0; at < sizeof bad_hashes / sizeof bad_hashes[0]; at++) {
        char *url = (char *)"sentinel";
        weivel_standard_refusal refusal = {0};
        CHECK(weivel_standard_mirror_expand("https://example.org/{sha256}", bad_hashes[at], &url, &refusal) ==
              WEIVEL_STATUS_REFUSED);
        CHECK(url == NULL);
        CHECK(refusal.code == WEIVEL_STANDARD_REFUSAL_INVALID_VALUE);
        weivel_standard_refusal_release(&refusal);
    }
}

static void refuses_a_bad_template_or_call(void) {
    char *url = NULL;
    CHECK(weivel_standard_mirror_expand("https://example.org/core", HASH, &url, NULL) == WEIVEL_STATUS_REFUSED);
    CHECK(weivel_standard_mirror_expand("{sha256}{sha256}", HASH, &url, NULL) == WEIVEL_STATUS_REFUSED);
    CHECK(url == NULL);
    CHECK(weivel_standard_mirror_expand(NULL, HASH, &url, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(weivel_standard_mirror_expand("{sha256}", NULL, &url, NULL) == WEIVEL_STATUS_MALFORMED);
    CHECK(weivel_standard_mirror_expand("{sha256}", HASH, NULL, NULL) == WEIVEL_STATUS_MALFORMED);
}

static void write_file(const char *text, size_t length) {
    FILE *file = fopen(WEIVEL_STANDARD_MIRRORS_FIXTURE, "wb");
    CHECK(file != NULL);
    if (file != NULL) {
        fwrite(text, 1, length, file);
        fclose(file);
    }
}

static void load_expecting_refusal(uint32_t code, const char *label) {
    weivel_standard_mirror_list list = {0};
    weivel_standard_refusal refusal = {0};
    CHECK(weivel_standard_load_mirrors(WEIVEL_STANDARD_MIRRORS_FIXTURE, &list, &refusal) == WEIVEL_STATUS_REFUSED);
    if (refusal.code != code) {
        fprintf(stderr, "%s: code %u, expected %u\n", label, (unsigned)refusal.code, (unsigned)code);
        check_failures++;
    }
    CHECK(list.count == 0 && list.storage == NULL);
    weivel_standard_refusal_release(&refusal);
}

static void loads_a_file(void) {
    weivel_standard_mirror_list list = {0};
    weivel_standard_refusal refusal = {0};
    const char *json = json_of("{'mirrors': ['file:///srv/core/{sha256}']}");
    size_t large_length = ((size_t)1 << 20) + 1;
    char *large = malloc(large_length);
    write_file(json, strlen(json));
    CHECK(weivel_standard_load_mirrors(WEIVEL_STANDARD_MIRRORS_FIXTURE, &list, &refusal) == WEIVEL_STATUS_OK);
    CHECK(list.count == 1);
    weivel_standard_mirror_list_release(&list);
    if (large != NULL) {
        memset(large, ' ', large_length);
        write_file(large, large_length);
        load_expecting_refusal(WEIVEL_STANDARD_REFUSAL_UNREADABLE, "too large");
        free(large);
    }
    json = json_of("{'mirrors': []}");
    write_file(json, strlen(json));
    load_expecting_refusal(WEIVEL_STANDARD_REFUSAL_INVALID_VALUE, "empty list");
    remove(WEIVEL_STANDARD_MIRRORS_FIXTURE);
    load_expecting_refusal(WEIVEL_STANDARD_REFUSAL_NOT_FOUND, "absent");
    CHECK(weivel_standard_load_mirrors(NULL, &list, &refusal) == WEIVEL_STATUS_MALFORMED);
    weivel_standard_refusal_release(&refusal);
}

int main(void) {
    reads_an_ordered_list();
    refuses_a_malformed_list();
    refuses_a_malformed_call();
    expands_a_template();
    refuses_a_bad_hash();
    refuses_a_bad_template_or_call();
    loads_a_file();
    return check_summary("mirrors_test");
}
