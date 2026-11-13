#ifndef WEIVEL_STANDARD_TEST_REPOSITORY_ID_SAMPLES_H
#define WEIVEL_STANDARD_TEST_REPOSITORY_ID_SAMPLES_H

#include <stddef.h>

typedef struct sample {
    const char *text;
    size_t length;
    int matches;
} sample;

#define TEXT(literal) literal, sizeof(literal) - 1

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

#endif
