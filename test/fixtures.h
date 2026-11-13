#ifndef WEIVEL_STANDARD_TEST_FIXTURES_H
#define WEIVEL_STANDARD_TEST_FIXTURES_H

#include <stddef.h>

#define FIXTURE_PATH_CAPACITY 1024

/** Creates `directory` (UTF-8) if it is absent and empties it of files and empty subdirectories. 1 on success. */
int fixture_fresh_directory(const char *directory);

/** Creates `directory` if it is absent. 1 on success. */
int fixture_make_directory(const char *directory);

/** Writes `text` to `directory/name`, replacing any file there. 1 on success. */
int fixture_write(const char *directory, const char *name, const char *text);

/** `directory/name` into `path`, which holds FIXTURE_PATH_CAPACITY bytes. */
void fixture_join(char *path, const char *directory, const char *name);

/** Removes `directory` if it exists and is empty, so a test can name one that does not exist. */
void fixture_remove_directory(const char *directory);

/**
 * Makes `path` unreadable to this process until fixture_unlock: an exclusive open on Windows, mode 000 elsewhere.
 * Answers 0 when the platform cannot (a process that reads every file regardless, as root does).
 */
int fixture_lock(const char *path);
void fixture_unlock(const char *path);

/** Creates `directory/name` as a symbolic link to `target`; 0 where this process may not make links. */
int fixture_link(const char *directory, const char *name, const char *target);

#endif
