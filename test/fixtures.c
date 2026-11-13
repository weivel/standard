#define _POSIX_C_SOURCE 200809L

#include "fixtures.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

void fixture_join(char *path, const char *directory, const char *name) {
    snprintf(path, FIXTURE_PATH_CAPACITY, "%s/%s", directory, name);
}

#ifdef _WIN32

static wchar_t *wide_of(const char *text) {
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    wchar_t *wide;
    if (count <= 0) {
        return NULL;
    }
    wide = malloc((size_t)count * sizeof *wide);
    if (wide != NULL && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, count) != count) {
        free(wide);
        wide = NULL;
    }
    return wide;
}

int fixture_make_directory(const char *directory) {
    wchar_t *wide = wide_of(directory);
    int made = wide != NULL && (CreateDirectoryW(wide, NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
    free(wide);
    return made;
}

static void empty_directory(const wchar_t *directory) {
    size_t length = wcslen(directory);
    wchar_t *pattern = malloc((length + 3) * sizeof *pattern);
    WIN32_FIND_DATAW data;
    HANDLE handle;
    if (pattern == NULL) {
        return;
    }
    memcpy(pattern, directory, length * sizeof *pattern);
    memcpy(pattern + length, L"/*", 3 * sizeof *pattern);
    handle = FindFirstFileW(pattern, &data);
    free(pattern);
    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        size_t name_length = wcslen(data.cFileName);
        wchar_t *child;
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) {
            continue;
        }
        child = malloc((length + name_length + 2) * sizeof *child);
        if (child == NULL) {
            continue;
        }
        memcpy(child, directory, length * sizeof *child);
        child[length] = L'/';
        memcpy(child + length + 1, data.cFileName, (name_length + 1) * sizeof *child);
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            RemoveDirectoryW(child);
        } else {
            DeleteFileW(child);
        }
        free(child);
    } while (FindNextFileW(handle, &data));
    FindClose(handle);
}

int fixture_fresh_directory(const char *directory) {
    wchar_t *wide;
    if (!fixture_make_directory(directory)) {
        return 0;
    }
    wide = wide_of(directory);
    if (wide == NULL) {
        return 0;
    }
    empty_directory(wide);
    free(wide);
    return 1;
}

int fixture_write(const char *directory, const char *name, const char *text) {
    char path[FIXTURE_PATH_CAPACITY];
    wchar_t *wide;
    HANDLE handle;
    DWORD written = 0;
    size_t length = strlen(text);
    fixture_join(path, directory, name);
    wide = wide_of(path);
    if (wide == NULL) {
        return 0;
    }
    handle = CreateFileW(wide, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    free(wide);
    if (handle == INVALID_HANDLE_VALUE) {
        return 0;
    }
    WriteFile(handle, text, (DWORD)length, &written, NULL);
    CloseHandle(handle);
    return written == (DWORD)length;
}

void fixture_remove_directory(const char *directory) {
    wchar_t *wide = wide_of(directory);
    if (wide != NULL) {
        RemoveDirectoryW(wide);
    }
    free(wide);
}

static HANDLE locked = INVALID_HANDLE_VALUE;

int fixture_lock(const char *path) {
    wchar_t *wide = wide_of(path);
    if (wide == NULL) {
        return 0;
    }
    locked = CreateFileW(wide, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    free(wide);
    return locked != INVALID_HANDLE_VALUE;
}

void fixture_unlock(const char *path) {
    (void)path;
    if (locked != INVALID_HANDLE_VALUE) {
        CloseHandle(locked);
        locked = INVALID_HANDLE_VALUE;
    }
}

#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif

static int make_link(const char *directory, const char *name, const char *target, DWORD flags) {
    char path[FIXTURE_PATH_CAPACITY];
    wchar_t *wide_path;
    wchar_t *wide_target;
    int linked = 0;
    fixture_join(path, directory, name);
    wide_path = wide_of(path);
    wide_target = wide_of(target);
    if (wide_path != NULL && wide_target != NULL) {
        DeleteFileW(wide_path);
        RemoveDirectoryW(wide_path);
        linked = CreateSymbolicLinkW(wide_path, wide_target, flags | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != 0;
    }
    free(wide_path);
    free(wide_target);
    return linked;
}

int fixture_link(const char *directory, const char *name, const char *target) {
    return make_link(directory, name, target, 0);
}

int fixture_link_directory(const char *directory, const char *name, const char *target) {
    return make_link(directory, name, target, SYMBOLIC_LINK_FLAG_DIRECTORY);
}

#else

int fixture_make_directory(const char *directory) {
    return mkdir(directory, 0755) == 0 || errno == EEXIST;
}

int fixture_fresh_directory(const char *directory) {
    DIR *handle;
    struct dirent *item;
    if (!fixture_make_directory(directory)) {
        return 0;
    }
    handle = opendir(directory);
    if (handle == NULL) {
        return 0;
    }
    while ((item = readdir(handle)) != NULL) {
        char path[FIXTURE_PATH_CAPACITY];
        struct stat status;
        if (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0) {
            continue;
        }
        fixture_join(path, directory, item->d_name);
        if (lstat(path, &status) == 0 && S_ISDIR(status.st_mode)) {
            rmdir(path);
        } else {
            unlink(path);
        }
    }
    closedir(handle);
    return 1;
}

int fixture_write(const char *directory, const char *name, const char *text) {
    char path[FIXTURE_PATH_CAPACITY];
    FILE *file;
    size_t length = strlen(text);
    int written;
    fixture_join(path, directory, name);
    unlink(path);
    file = fopen(path, "wb");
    if (file == NULL) {
        return 0;
    }
    written = fwrite(text, 1, length, file) == length;
    return fclose(file) == 0 && written;
}

void fixture_remove_directory(const char *directory) {
    rmdir(directory);
}

int fixture_lock(const char *path) {
    int descriptor;
    if (chmod(path, 0) != 0) {
        return 0;
    }
    descriptor = open(path, O_RDONLY);
    if (descriptor >= 0) {
        close(descriptor);
        chmod(path, 0644);
        return 0;
    }
    return 1;
}

void fixture_unlock(const char *path) {
    chmod(path, 0644);
}

int fixture_link(const char *directory, const char *name, const char *target) {
    char path[FIXTURE_PATH_CAPACITY];
    fixture_join(path, directory, name);
    unlink(path);
    return symlink(target, path) == 0;
}

int fixture_link_directory(const char *directory, const char *name, const char *target) {
    return fixture_link(directory, name, target);
}

#endif
