#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include <stddef.h>

void memory_copy(uint8_t *source, uint8_t *dest, size_t nbytes);
void* memory_set(void *dest, uint8_t val, size_t len);

/* At this stage there is no 'free' implemented. */
uint32_t kmalloc(size_t size, int align, uint32_t *phys_addr);
void* aligned_alloc(size_t alignment, size_t size);
uintptr_t get_physical_address(void* virtual_address);

#endif
