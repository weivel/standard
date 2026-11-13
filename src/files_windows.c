#include "files.h"

#include "allocation.h"

#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define READ_CHUNK ((DWORD)1 << 30)

/** `text` with `suffix` appended, as UTF-16, into `*wide`; UNREADABLE when `text` is not UTF-8. */
static weivel_standard_files_outcome wide_of(const char *text, const wchar_t *suffix, wchar_t **wide) {
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    size_t suffix_length = wcslen(suffix);
    *wide = NULL;
    if (count <= 0) {
        return WEIVEL_STANDARD_FILES_UNREADABLE;
    }
    *wide = weivel_standard_memory_allocate(((size_t)count + suffix_length) * sizeof **wide);
    if (*wide == NULL) {
        return WEIVEL_STANDARD_FILES_OUT_OF_MEMORY;
    }
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, *wide, count);
    memcpy(*wide + count - 1, suffix, (suffix_length + 1) * sizeof **wide);
    return WEIVEL_STANDARD_FILES_DONE;
}

static int is_not_found(DWORD error) {
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND || error == ERROR_INVALID_NAME;
}

static int is_dot_entry(const wchar_t *name) {
    return wcscmp(name, L".") == 0 || wcscmp(name, L"..") == 0;
}

static int is_listed(const WIN32_FIND_DATAW *data) {
    return (data->dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0;
}

/** 1 when added or left out for a spelling UTF-8 cannot carry, 0 when memory runs out. */
static int add_wide_name(weivel_standard_file_names *names, const wchar_t *name) {
    int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, name, -1, NULL, 0, NULL, NULL);
    char *utf8;
    int added;
    if (length <= 0) {
        return 1;
    }
    utf8 = weivel_standard_memory_allocate((size_t)length);
    if (utf8 == NULL) {
        return 0;
    }
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, name, -1, utf8, length, NULL, NULL);
    added = weivel_standard_file_names_add(names, utf8, (size_t)length - 1);
    weivel_standard_memory_release(utf8);
    return added;
}

static weivel_standard_files_outcome directory_outcome(const char *directory) {
    wchar_t *wide;
    weivel_standard_files_outcome outcome = wide_of(directory, L"", &wide);
    DWORD attributes;
    DWORD error;
    if (outcome != WEIVEL_STANDARD_FILES_DONE) {
        return outcome;
    }
    attributes = GetFileAttributesW(wide);
    error = GetLastError();
    weivel_standard_memory_release(wide);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return is_not_found(error) ? WEIVEL_STANDARD_FILES_MISSING : WEIVEL_STANDARD_FILES_UNREADABLE;
    }
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) ? WEIVEL_STANDARD_FILES_DONE
                                                   : WEIVEL_STANDARD_FILES_NOT_A_DIRECTORY;
}

static weivel_standard_files_outcome list_entries(HANDLE handle, WIN32_FIND_DATAW *data,
                                                  weivel_standard_file_names *names) {
    do {
        if (!is_dot_entry(data->cFileName) && is_listed(data) && !add_wide_name(names, data->cFileName)) {
            return WEIVEL_STANDARD_FILES_OUT_OF_MEMORY;
        }
    } while (FindNextFileW(handle, data));
    return GetLastError() == ERROR_NO_MORE_FILES ? WEIVEL_STANDARD_FILES_DONE : WEIVEL_STANDARD_FILES_UNREADABLE;
}

weivel_standard_files_outcome weivel_standard_files_list(const char *directory, weivel_standard_file_names *names) {
    weivel_standard_files_outcome outcome = directory_outcome(directory);
    WIN32_FIND_DATAW data;
    wchar_t *pattern;
    HANDLE handle;
    names->items = NULL;
    names->count = 0;
    names->capacity = 0;
    if (outcome != WEIVEL_STANDARD_FILES_DONE) {
        return outcome;
    }
    outcome = wide_of(directory, L"/*", &pattern);
    if (outcome != WEIVEL_STANDARD_FILES_DONE) {
        return outcome;
    }
    handle = FindFirstFileW(pattern, &data);
    weivel_standard_memory_release(pattern);
    if (handle == INVALID_HANDLE_VALUE) {
        return GetLastError() == ERROR_FILE_NOT_FOUND ? WEIVEL_STANDARD_FILES_DONE : WEIVEL_STANDARD_FILES_UNREADABLE;
    }
    outcome = list_entries(handle, &data, names);
    FindClose(handle);
    if (outcome != WEIVEL_STANDARD_FILES_DONE) {
        weivel_standard_file_names_release(names);
    }
    return outcome;
}

static weivel_standard_file_kind kind_of(const wchar_t *path) {
    DWORD attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return is_not_found(GetLastError()) ? WEIVEL_STANDARD_FILE_NOTHING : WEIVEL_STANDARD_FILE_UNKNOWN;
    }
    return (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ? WEIVEL_STANDARD_FILE_OTHER
                                                                                     : WEIVEL_STANDARD_FILE_REGULAR;
}

weivel_standard_files_outcome weivel_standard_files_kind(const char *path, weivel_standard_file_kind *kind) {
    wchar_t *wide;
    weivel_standard_files_outcome outcome = wide_of(path, L"", &wide);
    *kind = WEIVEL_STANDARD_FILE_UNKNOWN;
    if (outcome == WEIVEL_STANDARD_FILES_OUT_OF_MEMORY) {
        return outcome;
    }
    if (outcome == WEIVEL_STANDARD_FILES_DONE) {
        *kind = kind_of(wide);
        weivel_standard_memory_release(wide);
    }
    return WEIVEL_STANDARD_FILES_DONE;
}

static weivel_standard_files_outcome read_open(HANDLE handle, char **bytes, size_t *length) {
    LARGE_INTEGER size;
    size_t total;
    size_t done = 0;
    char *buffer;
    if (!GetFileSizeEx(handle, &size) || size.QuadPart < 0 || (unsigned long long)size.QuadPart >= SIZE_MAX) {
        return WEIVEL_STANDARD_FILES_UNREADABLE;
    }
    total = (size_t)size.QuadPart;
    buffer = weivel_standard_memory_allocate(total + 1);
    if (buffer == NULL) {
        return WEIVEL_STANDARD_FILES_OUT_OF_MEMORY;
    }
    while (done < total) {
        DWORD wanted = total - done > READ_CHUNK ? READ_CHUNK : (DWORD)(total - done);
        DWORD read = 0;
        if (!ReadFile(handle, buffer + done, wanted, &read, NULL) || read == 0) {
            weivel_standard_memory_release(buffer);
            return WEIVEL_STANDARD_FILES_UNREADABLE;
        }
        done += read;
    }
    buffer[total] = '\0';
    *bytes = buffer;
    *length = total;
    return WEIVEL_STANDARD_FILES_DONE;
}

weivel_standard_files_outcome weivel_standard_files_read(const char *path, char **bytes, size_t *length) {
    wchar_t *wide;
    weivel_standard_files_outcome outcome = wide_of(path, L"", &wide);
    HANDLE handle;
    *bytes = NULL;
    *length = 0;
    if (outcome != WEIVEL_STANDARD_FILES_DONE) {
        return outcome;
    }
    handle = CreateFileW(wide, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    weivel_standard_memory_release(wide);
    if (handle == INVALID_HANDLE_VALUE) {
        return WEIVEL_STANDARD_FILES_UNREADABLE;
    }
    outcome = read_open(handle, bytes, length);
    CloseHandle(handle);
    return outcome;
}
