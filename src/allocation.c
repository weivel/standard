#include "allocation.h"

#include <stdlib.h>

#ifdef WEIVEL_STANDARD_TESTING

static uint64_t countdown;
static int failed;
static int64_t live_blocks;

static int fails_now(void) {
    if (countdown == 0) {
        return 0;
    }
    countdown--;
    if (countdown > 0) {
        return 0;
    }
    failed = 1;
    return 1;
}

static void *counted(void *block) {
    if (block != NULL) {
        live_blocks++;
    }
    return block;
}

void *weivel_standard_memory_allocate(size_t bytes) {
    return fails_now() ? NULL : counted(malloc(bytes));
}

void *weivel_standard_memory_allocate_zeroed(size_t count, size_t bytes) {
    return fails_now() ? NULL : counted(calloc(count, bytes));
}

void *weivel_standard_memory_reallocate(void *block, size_t bytes) {
    void *grown;
    if (fails_now()) {
        return NULL;
    }
    grown = realloc(block, bytes);
    return block == NULL ? counted(grown) : grown;
}

void weivel_standard_memory_release(void *block) {
    if (block != NULL) {
        live_blocks--;
    }
    free(block);
}

void weivel_standard_memory_fail_at(uint64_t nth) {
    countdown = nth;
    failed = 0;
}

int weivel_standard_memory_failed(void) {
    return failed;
}

int64_t weivel_standard_memory_live_blocks(void) {
    return live_blocks;
}

#else

void *weivel_standard_memory_allocate(size_t bytes) {
    return malloc(bytes);
}

void *weivel_standard_memory_allocate_zeroed(size_t count, size_t bytes) {
    return calloc(count, bytes);
}

void *weivel_standard_memory_reallocate(void *block, size_t bytes) {
    return realloc(block, bytes);
}

void weivel_standard_memory_release(void *block) {
    free(block);
}

#endif
