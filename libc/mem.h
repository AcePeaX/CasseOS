#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include <stddef.h>

void memory_copy(void *dest, const void *source, size_t nbytes);
#define memcpy memory_copy

void* memory_set(void *dest, uint8_t val, size_t len);
#define memset memory_set


/* At this stage there is no 'free' implemented. */
uint32_t kmalloc(size_t size, int align, uint32_t *phys_addr);
void* aligned_alloc(size_t alignment, size_t size);
uintptr_t get_physical_address(void* virtual_address);
void aligned_free(void* ptr);



#endif
