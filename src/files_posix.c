#define _POSIX_C_SOURCE 200809L
#if defined(__APPLE__)
/** Apple's headers hide O_NOFOLLOW under a strict _POSIX_C_SOURCE unless this is defined too. */
#define _DARWIN_C_SOURCE
#endif

#include "files.h"

#include "allocation.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FIRST_READ_CAPACITY 4096

static char *joined(const char *directory, const char *name) {
    size_t directory_length = strlen(directory);
    size_t name_length = strlen(name);
    char *path = weivel_standard_memory_allocate(directory_length + name_length + 2);
    if (path != NULL) {
        memcpy(path, directory, directory_length);
        path[directory_length] = '/';
        memcpy(path + directory_length + 1, name, name_length + 1);
    }
    return path;
}

static int is_dot_entry(const char *name) {
    return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

/** A regular file, or an entry that could not be looked at, which reading then reports. */
static int is_listed(const char *directory, const char *name, int *out_of_memory) {
    char *path = joined(directory, name);
    struct stat status;
    int listed;
    if (path == NULL) {
        *out_of_memory = 1;
        return 0;
    }
    listed = lstat(path, &status) == 0 ? S_ISREG(status.st_mode) : errno != ENOENT;
    weivel_standard_memory_release(path);
    return listed;
}

static weivel_standard_files_outcome opening_outcome(int error) {
    if (error == ENOENT) {
        return WEIVEL_STANDARD_FILES_MISSING;
    }
    return error == ENOTDIR ? WEIVEL_STANDARD_FILES_NOT_A_DIRECTORY : WEIVEL_STANDARD_FILES_UNREADABLE;
}

static weivel_standard_files_outcome list_entries(DIR *handle, const char *directory,
                                                  weivel_standard_file_names *names) {
    for (;;) {
        struct dirent *item;
        int out_of_memory = 0;
        errno = 0;
        item = readdir(handle);
        if (item == NULL) {
            return errno == 0 ? WEIVEL_STANDARD_FILES_DONE : WEIVEL_STANDARD_FILES_UNREADABLE;
        }
        if (is_dot_entry(item->d_name)) {
            continue;
        }
        if (is_listed(directory, item->d_name, &out_of_memory) &&
            !weivel_standard_file_names_add(names, item->d_name, strlen(item->d_name))) {
            return WEIVEL_STANDARD_FILES_OUT_OF_MEMORY;
        }
        if (out_of_memory) {
            return WEIVEL_STANDARD_FILES_OUT_OF_MEMORY;
        }
    }
}

weivel_standard_files_outcome weivel_standard_files_list(const char *directory, weivel_standard_file_names *names) {
    weivel_standard_files_outcome outcome;
    DIR *handle;
    names->items = NULL;
    names->count = 0;
    names->capacity = 0;
    handle = opendir(directory);
    if (handle == NULL) {
        return opening_outcome(errno);
    }
    outcome = list_entries(handle, directory, names);
    closedir(handle);
    if (outcome != WEIVEL_STANDARD_FILES_DONE) {
        weivel_standard_file_names_release(names);
    }
    return outcome;
}

weivel_standard_files_outcome weivel_standard_files_kind(const char *path, weivel_standard_file_kind *kind) {
    struct stat status;
    if (lstat(path, &status) != 0) {
        *kind = errno == ENOENT || errno == ENOTDIR ? WEIVEL_STANDARD_FILE_NOTHING : WEIVEL_STANDARD_FILE_UNKNOWN;
    } else {
        *kind = S_ISREG(status.st_mode)   ? WEIVEL_STANDARD_FILE_REGULAR
                : S_ISDIR(status.st_mode) ? WEIVEL_STANDARD_FILE_DIRECTORY
                : S_ISLNK(status.st_mode) ? WEIVEL_STANDARD_FILE_LINK
                                          : WEIVEL_STANDARD_FILE_OTHER;
    }
    return WEIVEL_STANDARD_FILES_DONE;
}

/** Grows to at most one byte past the cap and a NUL, so a read past the cap is seen without reading further. */
static int grow(char **buffer, size_t *capacity) {
    size_t wanted = *capacity * 2;
    char *grown;
    if (wanted > WEIVEL_STANDARD_FILES_LARGEST_READ + 2) {
        wanted = WEIVEL_STANDARD_FILES_LARGEST_READ + 2;
    }
    grown = weivel_standard_memory_reallocate(*buffer, wanted);
    if (grown == NULL) {
        return 0;
    }
    *buffer = grown;
    *capacity = wanted;
    return 1;
}

static weivel_standard_files_outcome read_open(int descriptor, char **bytes, size_t *length) {
    size_t capacity = FIRST_READ_CAPACITY;
    size_t done = 0;
    char *buffer = weivel_standard_memory_allocate(capacity);
    if (buffer == NULL) {
        return WEIVEL_STANDARD_FILES_OUT_OF_MEMORY;
    }
    for (;;) {
        ssize_t got;
        if (done + 1 == capacity && !grow(&buffer, &capacity)) {
            weivel_standard_memory_release(buffer);
            return WEIVEL_STANDARD_FILES_OUT_OF_MEMORY;
        }
        got = read(descriptor, buffer + done, capacity - done - 1);
        if (got < 0 && errno == EINTR) {
            continue;
        }
        if (got < 0) {
            weivel_standard_memory_release(buffer);
            return WEIVEL_STANDARD_FILES_UNREADABLE;
        }
        if (got == 0) {
            break;
        }
        done += (size_t)got;
        if (done > WEIVEL_STANDARD_FILES_LARGEST_READ) {
            weivel_standard_memory_release(buffer);
            return WEIVEL_STANDARD_FILES_TOO_LARGE;
        }
    }
    buffer[done] = '\0';
    *bytes = buffer;
    *length = done;
    return WEIVEL_STANDARD_FILES_DONE;
}

weivel_standard_files_outcome weivel_standard_files_read(const char *path, char **bytes, size_t *length) {
    weivel_standard_files_outcome outcome;
    struct stat status;
    int descriptor;
    *bytes = NULL;
    *length = 0;
    descriptor = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (descriptor < 0) {
        return WEIVEL_STANDARD_FILES_UNREADABLE;
    }
    if (fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode)) {
        close(descriptor);
        return WEIVEL_STANDARD_FILES_UNREADABLE;
    }
    if ((unsigned long long)status.st_size > WEIVEL_STANDARD_FILES_LARGEST_READ) {
        close(descriptor);
        return WEIVEL_STANDARD_FILES_TOO_LARGE;
    }
    outcome = read_open(descriptor, bytes, length);
    close(descriptor);
    return outcome;
}
