#include "loader.h"

#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

typedef void (*any_function)(void);

int described_library_open(const char *path, described_library *library) {
#if defined(_WIN32)
    library->module = (void *)LoadLibraryA(path);
#else
    library->module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
    return library->module != NULL;
}

/** @implNote dlsym answers an object pointer, which ISO C forbids casting to a function pointer, so it is copied. */
static any_function find_function(described_library library, const char *name) {
    any_function function = NULL;
#if defined(_WIN32)
    FARPROC found = GetProcAddress((HMODULE)library.module, name);
#else
    void *found = dlsym(library.module, name);
#endif
    memcpy(&function, &found, sizeof function);
    return function;
}

described_definition_symbol described_library_definition(described_library library, const char *name) {
    return (described_definition_symbol)find_function(library, name);
}

described_description_symbol described_library_description(described_library library, const char *name) {
    return (described_description_symbol)find_function(library, name);
}

int described_library_close(described_library library) {
#if defined(_WIN32)
    return FreeLibrary((HMODULE)library.module) != 0;
#else
    return dlclose(library.module) == 0;
#endif
}
