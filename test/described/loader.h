#ifndef WEIVEL_STANDARD_TEST_DESCRIBED_LOADER_H
#define WEIVEL_STANDARD_TEST_DESCRIBED_LOADER_H

/**
 * @file
 * Loads the described library at run time, as a host does, so its symbols are proven exported across a shared-library
 * boundary rather than resolved at link time.
 */

#include "weivel/description.h"

typedef struct described_library {
    void *module;
} described_library;

typedef const void *(*described_definition_symbol)(void);
typedef const weivel_standard_description *(*described_description_symbol)(void);

/** 0 when the library at `path` cannot be loaded. */
int described_library_open(const char *path, described_library *library);

/** The exported function `name` answering a definition, or NULL when the library exports none by that name. */
described_definition_symbol described_library_definition(described_library library, const char *name);

/** The exported function `name` answering a description, or NULL when the library exports none by that name. */
described_description_symbol described_library_description(described_library library, const char *name);

/** 0 when the library cannot be closed. */
int described_library_close(described_library library);

#endif
