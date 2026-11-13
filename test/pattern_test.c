#include "check.h"
#include "patterns.h"

typedef struct sample {
    const char *text;
    size_t length;
    int matches;
} sample;

#define TEXT(literal) literal, sizeof(literal) - 1

static void check_samples(const char *label, weivel_standard_pattern_matcher matches, const sample *samples,
                          size_t count) {
    size_t at;
    for (at = 0; at < count; at++) {
        if (matches(samples[at].text, samples[at].length) != samples[at].matches) {
            fprintf(stderr, "%s: \"%s\" should %smatch\n", label, samples[at].text, samples[at].matches ? "" : "not ");
            check_failures++;
        }
    }
}

static const sample IMPORT_NAMES[] = {
    {TEXT("a"), 1},          {TEXT("store.cache-2"), 1}, {TEXT("package-lock"), 1}, {TEXT("packages"), 1},
    {TEXT("console.foo"), 1}, {TEXT("con-x"), 1},        {TEXT("com0"), 1},         {TEXT("com10"), 1},
    {TEXT("lpt"), 1},        {TEXT("nulls"), 1},         {TEXT(""), 0},             {TEXT("package"), 0},
    {TEXT("con"), 0},        {TEXT("con.foo"), 0},       {TEXT("nul.x"), 0},        {TEXT("aux"), 0},
    {TEXT("prn.a.b"), 0},    {TEXT("com1"), 0},          {TEXT("lpt9.x"), 0},       {TEXT("Upper"), 0},
    {TEXT("a--b"), 0},       {TEXT("a.-b"), 0},          {TEXT("-a"), 0},           {TEXT("a."), 0},
    {TEXT("a_b"), 0},        {TEXT("a b"), 0},           {TEXT("a\nb"), 0},         {TEXT("a\0b"), 0},
};

static const sample ENTRIES[] = {
    {TEXT(""), 1},
    {TEXT("dist/index.js"), 1},
    {TEXT("lib/a..b/..."), 1},
    {TEXT("..x"), 1},
    {TEXT("x.."), 1},
    {TEXT("a/.hidden"), 1},
    {TEXT("caf\xc3\xa9.so"), 1},
    {TEXT("1:x"), 1},
    {TEXT("/abs"), 0},
    {TEXT("\\abs"), 0},
    {TEXT("C:lib"), 0},
    {TEXT("z:"), 0},
    {TEXT(".."), 0},
    {TEXT("../x.js"), 0},
    {TEXT("lib/.."), 0},
    {TEXT("lib\\..\\x"), 0},
    {TEXT("a/../b"), 0},
    {TEXT("a\tb"), 0},
    {TEXT("a\x7f"), 0},
    {TEXT("a\xc2\x85"), 0},
    {TEXT("a\xc2\x9f"), 0},
    {TEXT("a\xc2\xa0"), 1},
    {TEXT("a\xe2\x80\xa8"), 0},
    {TEXT("a\xe2\x80\xa9"), 0},
    {TEXT("a\xe2\x80\xaa"), 1},
    {TEXT("a\0b"), 0},
};

static const sample REPOSITORY_IDS[] = {
    {TEXT("forge:1434954468"), 1},
    {TEXT("gitea@git.example.com:123"), 1},
    {TEXT("local:plugin-stub"), 1},
    {TEXT("forge:1361969293/kernel"), 1},
    {TEXT("forge:1/a..b"), 1},
    {TEXT("forge:1/.hidden"), 1},
    {TEXT("forge:1/..."), 1},
    {TEXT("local:Caf\xc3\xa9"), 1},
    {TEXT("local:a:b/x/y"), 1},
    {TEXT("a-1:0/x_y.z"), 1},
    {TEXT("local:12"), 1},
    {TEXT("calendar"), 0},
    {TEXT("forge:1/../calendar_identity"), 0},
    {TEXT("forge:1/a/../b"), 0},
    {TEXT("forge:1/.."), 0},
    {TEXT("forge:abc"), 0},
    {TEXT("forge:1/."), 0},
    {TEXT("forge:"), 0},
    {TEXT("Forge:1"), 0},
    {TEXT("forge:1\n"), 0},
    {TEXT("forge:1/"), 0},
    {TEXT("forge:1//x"), 0},
    {TEXT("forge:1/X"), 0},
    {TEXT("forge@:1"), 0},
    {TEXT("forge@Host:1"), 0},
    {TEXT("1forge:1"), 0},
    {TEXT("local:"), 0},
    {TEXT("local:/x"), 0},
    {TEXT("local:a b"), 0},
    {TEXT("local:a\xc2\xa0"), 0},
    {TEXT("local:a\xe3\x80\x80"), 0},
    {TEXT("local:a\xef\xbb\xbf"), 0},
    {TEXT("local:a\xe2\x80\x8b"), 1},
};

static const sample OWNER_AND_NAMES[] = {
    {TEXT("vendor/runtime"), 1}, {TEXT("Ven.dor/..."), 1}, {TEXT("a/.x"), 1},   {TEXT("0_-./x"), 1},
    {TEXT("vendor"), 0},         {TEXT("vendor/."), 0},    {TEXT("vendor/.."), 0}, {TEXT("forge:1/x"), 0},
    {TEXT(".vendor/x"), 0},      {TEXT("a/b/c"), 0},       {TEXT("a/"), 0},     {TEXT("/a"), 0},
    {TEXT("a/b\n"), 0},          {TEXT(""), 0},
};

int main(void) {
    check_samples("importName", weivel_standard_matches_import_name, IMPORT_NAMES,
                  sizeof IMPORT_NAMES / sizeof IMPORT_NAMES[0]);
    check_samples("entry", weivel_standard_matches_entry, ENTRIES, sizeof ENTRIES / sizeof ENTRIES[0]);
    check_samples("repository id", weivel_standard_matches_repository_id, REPOSITORY_IDS,
                  sizeof REPOSITORY_IDS / sizeof REPOSITORY_IDS[0]);
    check_samples("owner and name", weivel_standard_matches_owner_and_name, OWNER_AND_NAMES,
                  sizeof OWNER_AND_NAMES / sizeof OWNER_AND_NAMES[0]);
    return check_summary("pattern_test");
}
