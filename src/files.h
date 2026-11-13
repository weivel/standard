#ifndef WEIVEL_STANDARD_FILES_H
#define WEIVEL_STANDARD_FILES_H

#include <stddef.h>

/** The largest file read: a manifest is small, and one huge file must be refused alone, not exhaust memory. */
#define WEIVEL_STANDARD_FILES_LARGEST_READ ((size_t)1 << 20)

/**
 * The standard's only reach into the file system, one implementation per platform. Paths are UTF-8; a link is never
 * followed, and everything is allocated through allocation.h.
 */
typedef enum weivel_standard_files_outcome {
    WEIVEL_STANDARD_FILES_DONE = 0,
    WEIVEL_STANDARD_FILES_MISSING = 1,
    WEIVEL_STANDARD_FILES_NOT_A_DIRECTORY = 2,
    WEIVEL_STANDARD_FILES_UNREADABLE = 3,
    WEIVEL_STANDARD_FILES_OUT_OF_MEMORY = 4,
    /** The file is larger than WEIVEL_STANDARD_FILES_LARGEST_READ. */
    WEIVEL_STANDARD_FILES_TOO_LARGE = 5
} weivel_standard_files_outcome;

typedef enum weivel_standard_file_kind {
    WEIVEL_STANDARD_FILE_NOTHING = 0,
    WEIVEL_STANDARD_FILE_REGULAR = 1,
    WEIVEL_STANDARD_FILE_DIRECTORY = 2,
    /** A symbolic link, or on Windows any reparse point, a junction or a cloud placeholder among them. */
    WEIVEL_STANDARD_FILE_LINK = 3,
    /** A device, a pipe: something there that is none of the above. */
    WEIVEL_STANDARD_FILE_OTHER = 4,
    /** Something may be there and could not be looked at. */
    WEIVEL_STANDARD_FILE_UNKNOWN = 5
} weivel_standard_file_kind;

typedef struct weivel_standard_file_names {
    char **items;
    size_t count;
    size_t capacity;
} weivel_standard_file_names;

/**
 * The names of the entries directly in `directory` that are regular files or could not be looked at, in no order.
 * On Windows a name is converted from UTF-16, one that is not valid UTF-16 is left out, and every reparse point, a
 * cloud placeholder file among them, is skipped; on POSIX a name is its raw bytes, which the caller's importName check
 * filters. `names` is empty on every answer but DONE.
 */
weivel_standard_files_outcome weivel_standard_files_list(const char *directory, weivel_standard_file_names *names);

/**
 * Reads a whole regular file into `*bytes`, `*length` bytes and a NUL; DONE, UNREADABLE, TOO_LARGE (checked before
 * allocating) or OUT_OF_MEMORY.
 */
weivel_standard_files_outcome weivel_standard_files_read(const char *path, char **bytes, size_t *length);

/** What is at `path`, into `*kind`, without following a link; DONE, or OUT_OF_MEMORY. */
weivel_standard_files_outcome weivel_standard_files_kind(const char *path, weivel_standard_file_kind *kind);

/** Adds a copy of `length` bytes of `name`; 0 when memory runs out. */
int weivel_standard_file_names_add(weivel_standard_file_names *names, const char *name, size_t length);

void weivel_standard_file_names_release(weivel_standard_file_names *names);

#endif
