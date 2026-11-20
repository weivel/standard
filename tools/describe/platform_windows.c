#include "platform.h"

#include <stdio.h>

#include "allocation.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

static char *utf8_of(const wchar_t *wide) {
    int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, NULL, 0, NULL, NULL);
    char *text;
    if (length <= 0) {
        return NULL;
    }
    text = weivel_standard_memory_allocate((size_t)length);
    if (text != NULL && WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, text, length, NULL, NULL) <= 0) {
        weivel_standard_memory_release(text);
        text = NULL;
    }
    return text;
}

static wchar_t *wide_of(const char *text) {
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    wchar_t *wide;
    if (count <= 0) {
        return NULL;
    }
    wide = weivel_standard_memory_allocate((size_t)count * sizeof *wide);
    if (wide != NULL && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, count) <= 0) {
        weivel_standard_memory_release(wide);
        wide = NULL;
    }
    return wide;
}

void describe_arguments_release(char **arguments, size_t count) {
    size_t at;
    if (arguments == NULL) {
        return;
    }
    for (at = 0; at < count; at++) {
        weivel_standard_memory_release(arguments[at]);
    }
    weivel_standard_memory_release(arguments);
}

static int copy_arguments(wchar_t **wide, size_t total, char ***arguments) {
    size_t at;
    *arguments = weivel_standard_memory_allocate_zeroed(total, sizeof **arguments);
    if (*arguments == NULL) {
        return 0;
    }
    for (at = 0; at < total; at++) {
        (*arguments)[at] = utf8_of(wide[at]);
        if ((*arguments)[at] == NULL) {
            describe_arguments_release(*arguments, total);
            *arguments = NULL;
            return 0;
        }
    }
    return 1;
}

int describe_arguments(int argc, char **argv, char ***arguments, size_t *count) {
    int wide_count = 0;
    wchar_t **wide = CommandLineToArgvW(GetCommandLineW(), &wide_count);
    int copied;
    (void)argc;
    (void)argv;
    *arguments = NULL;
    *count = 0;
    if (wide == NULL || wide_count <= 0) {
        LocalFree(wide);
        return 0;
    }
    copied = copy_arguments(wide, (size_t)wide_count, arguments);
    LocalFree(wide);
    if (copied) {
        *count = (size_t)wide_count;
    }
    return copied;
}

int describe_write_file(const char *path, const char *bytes, size_t length) {
    wchar_t *wide = wide_of(path);
    FILE *file = NULL;
    errno_t opened;
    int written;
    if (wide == NULL) {
        return 0;
    }
    opened = _wfopen_s(&file, wide, L"wb");
    weivel_standard_memory_release(wide);
    if (opened != 0 || file == NULL) {
        return 0;
    }
    written = fwrite(bytes, 1, length, file) == length;
    return fclose(file) == 0 && written;
}

void describe_remove_file(const char *path) {
    wchar_t *wide = wide_of(path);
    if (wide != NULL) {
        DeleteFileW(wide);
        weivel_standard_memory_release(wide);
    }
}
