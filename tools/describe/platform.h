#ifndef WEIVEL_DESCRIBE_PLATFORM_H
#define WEIVEL_DESCRIBE_PLATFORM_H

/**
 * @file
 * What weivel-describe needs of its platform beyond the standard's reading of files: its arguments as UTF-8, which
 * is what the standard's paths are, and writing the source it made. One implementation per platform.
 */

#include <stddef.h>

/**
 * The program's arguments as UTF-8, into `*arguments` and `*count`: on Windows read again from the wide command line,
 * since `argv` there is in the ANSI code page; elsewhere copied from `argv` as they are. 0 when memory runs out or
 * the command line cannot be read.
 */
int describe_arguments(int argc, char **argv, char ***arguments, size_t *count);

void describe_arguments_release(char **arguments, size_t count);

/** Replaces the file at the UTF-8 `path` with `length` bytes, written as they are; 0 on any failure. */
int describe_write_file(const char *path, const char *bytes, size_t length);

/** Removes the file at the UTF-8 `path`, when there is one. */
void describe_remove_file(const char *path);

#endif
