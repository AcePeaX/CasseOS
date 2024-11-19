#include "mem.h"

void memory_copy(uint8_t *source, uint8_t *dest, size_t nbytes) {
    size_t i;
    for (i = 0; i < nbytes; i++) {
        *(dest + i) = *(source + i);
    }
}

void* memory_set(void *dest, uint8_t val, size_t len) {
    uint8_t *temp = (uint8_t *)dest;
    for ( ; len != 0; len--) *temp++ = val;
    return dest;
}


/* This should be computed at link time, but a hardcoded
 * value is fine for now. Remember that our kernel starts
 * at 0x1000 as defined on the Makefile */
uint32_t free_mem_addr = 0x10000;
/* Implementation is just a pointer to some free memory which
 * keeps growing */
uint32_t kmalloc(size_t size, int align, uint32_t *phys_addr) {
    /* Pages are aligned to 4K, or 0x1000 */
    if (align == 1 && (free_mem_addr & 0xFFFFF000)) {
        free_mem_addr &= 0xFFFFF000;
        free_mem_addr += 0x1000;
    }
    /* Save also the physical address */
    if (phys_addr) *phys_addr = free_mem_addr;

    uint32_t ret = free_mem_addr;
    free_mem_addr += size; /* Remember to increment the pointer */
    return ret;
}


#define ALLOCATOR_SIZE (1024 * 512) // 512 KB allocator buffer
static uint8_t allocator_buffer[ALLOCATOR_SIZE];
static size_t allocator_offset = 0;

void* aligned_alloc(size_t alignment, size_t size) {
    // Ensure alignment is a power of two
    if ((alignment & (alignment - 1)) != 0) {
        return NULL; // Invalid alignment
    }

    uintptr_t current_address = (uintptr_t)&allocator_buffer[allocator_offset];
    size_t padding = (alignment - (current_address % alignment)) % alignment;

    if (allocator_offset + padding + size > ALLOCATOR_SIZE) {
        return NULL; // Not enough memory
    }

    allocator_offset += padding;
    void* aligned_ptr = &allocator_buffer[allocator_offset];
    allocator_offset += size;

    return aligned_ptr;
}

uintptr_t get_physical_address(void* virtual_address) {
    // Implement this based on your paging structures
    // For now, assume identity mapping
    return (uintptr_t)virtual_address;
}