#ifndef WEIVEL_STANDARD_FILES_H
#define WEIVEL_STANDARD_FILES_H

#include <stddef.h>

/**
 * The standard's only reach into the file system, one implementation per platform. Paths are UTF-8; a link is never
 * followed, and everything is allocated through allocation.h.
 */
typedef enum weivel_standard_files_outcome {
    WEIVEL_STANDARD_FILES_DONE = 0,
    WEIVEL_STANDARD_FILES_MISSING = 1,
    WEIVEL_STANDARD_FILES_NOT_A_DIRECTORY = 2,
    WEIVEL_STANDARD_FILES_UNREADABLE = 3,
    WEIVEL_STANDARD_FILES_OUT_OF_MEMORY = 4
} weivel_standard_files_outcome;

typedef enum weivel_standard_file_kind {
    WEIVEL_STANDARD_FILE_NOTHING = 0,
    WEIVEL_STANDARD_FILE_REGULAR = 1,
    /** A directory, a link, a device: something there that is not a regular file. */
    WEIVEL_STANDARD_FILE_OTHER = 2,
    /** Something may be there and could not be looked at. */
    WEIVEL_STANDARD_FILE_UNKNOWN = 3
} weivel_standard_file_kind;

typedef struct weivel_standard_file_names {
    char **items;
    size_t count;
    size_t capacity;
} weivel_standard_file_names;

/**
 * The names of the entries directly in `directory` that are regular files or could not be looked at, in no order;
 * a name the platform cannot spell in UTF-8 is left out. `names` is empty on every answer but DONE.
 */
weivel_standard_files_outcome weivel_standard_files_list(const char *directory, weivel_standard_file_names *names);

/** Reads a whole regular file into `*bytes`, `*length` bytes and a NUL; DONE, UNREADABLE or OUT_OF_MEMORY. */
weivel_standard_files_outcome weivel_standard_files_read(const char *path, char **bytes, size_t *length);

/** What is at `path`, into `*kind`, without following a link; DONE, or OUT_OF_MEMORY. */
weivel_standard_files_outcome weivel_standard_files_kind(const char *path, weivel_standard_file_kind *kind);

/** Adds a copy of `length` bytes of `name`; 0 when memory runs out. */
int weivel_standard_file_names_add(weivel_standard_file_names *names, const char *name, size_t length);

void weivel_standard_file_names_release(weivel_standard_file_names *names);

#endif
