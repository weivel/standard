#include "files.h"

#include "allocation.h"

#include <string.h>

int weivel_standard_file_names_add(weivel_standard_file_names *names, const char *name, size_t length) {
    char *copy;
    if (names->count == names->capacity) {
        size_t wanted = names->capacity == 0 ? 16 : names->capacity * 2;
        char **grown = weivel_standard_memory_reallocate(names->items, wanted * sizeof *grown);
        if (grown == NULL) {
            return 0;
        }
        names->items = grown;
        names->capacity = wanted;
    }
    copy = weivel_standard_memory_allocate(length + 1);
    if (copy == NULL) {
        return 0;
    }
    memcpy(copy, name, length);
    copy[length] = '\0';
    names->items[names->count++] = copy;
    return 1;
}

void weivel_standard_file_names_release(weivel_standard_file_names *names) {
    size_t at;
    for (at = 0; at < names->count; at++) {
        weivel_standard_memory_release(names->items[at]);
    }
    weivel_standard_memory_release(names->items);
    names->items = NULL;
    names->count = 0;
    names->capacity = 0;
}
