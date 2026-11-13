#ifndef WEIVEL_STANDARD_ALLOCATION_H
#define WEIVEL_STANDARD_ALLOCATION_H

#include <stddef.h>
#include <stdint.h>

/**
 * The one allocator every file of the standard allocates through, so a build for the tests (WEIVEL_STANDARD_TESTING)
 * can count the blocks still live and make any one allocation fail.
 */
void *weivel_standard_memory_allocate(size_t bytes);
void *weivel_standard_memory_allocate_zeroed(size_t count, size_t bytes);
void *weivel_standard_memory_reallocate(void *block, size_t bytes);
void weivel_standard_memory_release(void *block);

#ifdef WEIVEL_STANDARD_TESTING
/** Makes the nth allocation from now fail, 1 being the next; 0 disarms. */
void weivel_standard_memory_fail_at(uint64_t nth);
int weivel_standard_memory_failed(void);
int64_t weivel_standard_memory_live_blocks(void);
#endif

#endif
