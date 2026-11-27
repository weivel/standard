#ifndef WEIVEL_STANDARD_TEST_REPOSITORY_ID_SAMPLES_H
#define WEIVEL_STANDARD_TEST_REPOSITORY_ID_SAMPLES_H

#include <stddef.h>

typedef struct sample {
    const char *text;
    size_t length;
    int matches;
} sample;

#define TEXT(literal) literal, sizeof(literal) - 1

#define SHA1_ROOT "4b825dc642cb6eb9a060e54bf8d69288fbee4904"
#define SHA256_ROOT "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"

static const sample REPOSITORY_IDS[] = {
    {TEXT("git:" SHA1_ROOT), 1},
    {TEXT("git:" SHA256_ROOT), 1},
    {TEXT("local:plugin-stub"), 1},
    {TEXT("git:" SHA1_ROOT "/kernel"), 1},
    {TEXT("git:" SHA1_ROOT "/a..b"), 1},
    {TEXT("git:" SHA1_ROOT "/.hidden"), 1},
    {TEXT("git:" SHA1_ROOT "/..."), 1},
    {TEXT("git:" SHA256_ROOT "/x_y.z/a-b"), 1},
    {TEXT("local:Caf\xc3\xa9"), 1},
    {TEXT("local:a:b/x/y"), 1},
    {TEXT("local:12"), 1},
    {TEXT("calendar"), 0},
    {TEXT("forge:123"), 0},
    {TEXT("forge@git.example.com:123"), 0},
    {TEXT("git:4B825DC642CB6EB9A060E54BF8D69288FBEE4904"), 0},
    {TEXT("git:" SHA1_ROOT "0"), 0},
    {TEXT("git:4b825dc642cb6eb9a060e54bf8d69288fbee490"), 0},
    {TEXT("git:" SHA256_ROOT "0"), 0},
    {TEXT("git:" SHA1_ROOT "g"), 0},
    {TEXT("git:"), 0},
    {TEXT("Git:" SHA1_ROOT), 0},
    {TEXT("git:" SHA1_ROOT "/../calendar"), 0},
    {TEXT("git:" SHA1_ROOT "/a/../b"), 0},
    {TEXT("git:" SHA1_ROOT "/.."), 0},
    {TEXT("git:" SHA1_ROOT "/."), 0},
    {TEXT("git:" SHA1_ROOT "\n"), 0},
    {TEXT("git:" SHA1_ROOT "/"), 0},
    {TEXT("git:" SHA1_ROOT "//x"), 0},
    {TEXT("git:" SHA1_ROOT "/X"), 0},
    {TEXT("local:"), 0},
    {TEXT("local:/x"), 0},
    {TEXT("local:a b"), 0},
    {TEXT("local:a\0b"), 0},
    {TEXT("local:a\xc2\xa0"), 0},
    {TEXT("local:a\xe3\x80\x80"), 0},
    {TEXT("local:a\xef\xbb\xbf"), 0},
    {TEXT("local:a\xe2\x80\x8b"), 1},
};

#endif
