#ifndef WEIVEL_DESCRIBE_PROGRAM_H
#define WEIVEL_DESCRIBE_PROGRAM_H

/**
 * @file
 * The weivel-describe program apart from reading its command line, so a test can run it in its own process.
 */

#include <stddef.h>

enum { DESCRIBE_EXIT_DESCRIBED = 0, DESCRIBE_EXIT_REFUSED = 1, DESCRIBE_EXIT_USAGE = 2, DESCRIBE_EXIT_FAILED = 3 };

/**
 * Runs weivel-describe over its UTF-8 `arguments`, the program's name first, and answers its exit code: DESCRIBED
 * with the source written; REFUSED, printing the refusal; USAGE, touching nothing; FAILED when memory runs out or the
 * source cannot be written. Every exit but DESCRIBED and USAGE removes whatever an earlier run left at `--output`, so
 * a stale source never outlives a run that could not replace it.
 */
int describe_program(char **arguments, size_t count);

#endif
