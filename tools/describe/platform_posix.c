#include "platform.h"

#include <stdio.h>

#include "allocation.h"
#include "reading.h"

void describe_arguments_release(char **arguments, size_t count) {
    size_t at;
    if (arguments == NULL) {
        return;
    }
    for (at = 0; at < count; at++) {
        weivel_standard_memory_release(arguments[at]);
    }
    weivel_standard_memory_release(arguments);
}

int describe_arguments(int argc, char **argv, char ***arguments, size_t *count) {
    size_t total = argc < 0 ? 0 : (size_t)argc;
    size_t at;
    *arguments = NULL;
    *count = 0;
    if (total == 0) {
        return 1;
    }
    *arguments = weivel_standard_memory_allocate_zeroed(total, sizeof **arguments);
    if (*arguments == NULL) {
        return 0;
    }
    for (at = 0; at < total; at++) {
        (*arguments)[at] = describe_copy_text(argv[at]);
        if ((*arguments)[at] == NULL) {
            describe_arguments_release(*arguments, total);
            *arguments = NULL;
            return 0;
        }
    }
    *count = total;
    return 1;
}

int describe_write_file(const char *path, const char *bytes, size_t length) {
    FILE *file = fopen(path, "wb");
    int written;
    if (file == NULL) {
        return 0;
    }
    written = fwrite(bytes, 1, length, file) == length;
    return fclose(file) == 0 && written;
}

void describe_remove_file(const char *path) {
    remove(path);
}
