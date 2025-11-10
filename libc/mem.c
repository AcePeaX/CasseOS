#include "mem.h"

void memory_copy(void *dest, const void *source, size_t nbytes) {
    uint8_t *d = (uint8_t*)dest;
    const uint8_t *s = (const uint8_t*)source;

    for (size_t i = 0; i < nbytes; i++) {
        d[i] = s[i];
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

/* ______________________________________________________________________________________ */
typedef struct AllocationHeader {
    size_t size;                        // Size of the allocated block
    struct AllocationHeader* next_free; // Pointer to the next free block
} AllocationHeader;


#define ALLOCATOR_SIZE (1024 * 512) // 512 KB allocator buffer
static uint8_t allocator_buffer[ALLOCATOR_SIZE];
static size_t allocator_offset = 0;

static AllocationHeader* free_list = NULL; // Start of the free list

void* aligned_alloc(size_t alignment, size_t size) {
    // Ensure alignment is at least the size of a pointer and a power of two
    if (alignment < sizeof(void*)) {
        alignment = sizeof(void*);
    }
    if ((alignment & (alignment - 1)) != 0) {
        return NULL; // Invalid alignment
    }

    // Adjust size to include the AllocationHeader
    size_t total_size = size + sizeof(AllocationHeader);

    // Search for a suitable free block
    AllocationHeader** current = &free_list;
    while (*current != NULL) {
        if ((*current)->size >= total_size) {
            // Found a suitable block
            AllocationHeader* alloc_header = *current;
            *current = alloc_header->next_free; // Remove from free list

            // Calculate the aligned user data address
            uintptr_t raw_address = (uintptr_t)(alloc_header + 1);
            uintptr_t aligned_address = (raw_address + (alignment - 1)) & ~(alignment - 1);
            size_t padding = aligned_address - raw_address;

            if (padding > 0) {
                // Need to adjust for alignment; create a new header
                AllocationHeader* new_header = (AllocationHeader*)(aligned_address - sizeof(AllocationHeader));
                new_header->size = alloc_header->size - padding;
                alloc_header = new_header;
            }

            // Return the aligned user data pointer
            return (void*)(alloc_header + 1);
        }

        current = &((*current)->next_free);
    }

    // No suitable free block found; allocate from the buffer
    uintptr_t current_address = (uintptr_t)&allocator_buffer[allocator_offset];

    // Ensure the allocation header is aligned
    size_t header_padding = (alignment - (current_address % alignment)) % alignment;
    if (header_padding < sizeof(AllocationHeader)) {
        header_padding += alignment;
    }
    current_address += header_padding;

    // Check for buffer overflow
    if (allocator_offset + header_padding + total_size > ALLOCATOR_SIZE) {
        return NULL; // Not enough memory
    }

    // Update the allocator offset
    allocator_offset += header_padding + total_size;

    // Set up the allocation header
    AllocationHeader* alloc_header = (AllocationHeader*)current_address;
    alloc_header->size = total_size;

    // Return the aligned user data pointer
    return (void*)(alloc_header + 1);
}

void aligned_free(void* ptr) {
    if (!ptr) {
        return; // Do nothing for NULL pointer
    }

    // Get the allocation header
    AllocationHeader* alloc_header = (AllocationHeader*)ptr - 1;

    // Add the block to the free list
    alloc_header->next_free = free_list;
    free_list = alloc_header;
}



uintptr_t get_physical_address(void* virtual_address) {
    // Implement this based on your paging structures
    // For now, assume identity mapping
    return (uintptr_t)virtual_address;
}