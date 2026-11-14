// SPDX-License-Identifier: GPL-3.0-or-later
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

/* The heap census (headless/heap_arenas.inc): this block belongs to the code that asked (041). */
void eden_heap_census_recharge(void *address, size_t bytes, const void *caller) __attribute__((weak));

/* Use the native boilerplate's proven allocation/free pair. */
void *__wrap_aligned_alloc(size_t alignment, size_t size) {
    if (!alignment || (alignment & (alignment - 1)) || size % alignment) {
        errno = EINVAL;
        return NULL;
    }
    void *address = NULL;
    int result = posix_memalign(&address, alignment < sizeof(void *) ? sizeof(void *) : alignment, size);
    if (result) {
        errno = result;
        fprintf(stderr, "posix_memalign failed: size=%zu alignment=%zu error=%d\n", size, alignment, result);
    }
    if (!result && eden_heap_census_recharge) eden_heap_census_recharge(address, size, __builtin_return_address(0));
    return result ? NULL : address;
}
