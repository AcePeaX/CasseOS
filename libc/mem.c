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

int memory_compare(const void* a, const void* b, size_t n) {
    const unsigned char* p1 = (const unsigned char*)a;
    const unsigned char* p2 = (const unsigned char*)b;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i])
            return (p1[i] < p2[i]) ? -1 : 1;
    }
    return 0;
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
    /* Ensure power-of-two alignment and minimum pointer size. */
    if (alignment < sizeof(void*)) {
        alignment = sizeof(void*);
    }
    if ((alignment & (alignment - 1)) != 0) {
        return NULL;
    }

    /* total bytes including header and padding */
    size_t total_size = size + sizeof(AllocationHeader) + alignment;

    /* Walk free list first (simple first-fit). */
    for (AllocationHeader **cur = &free_list; *cur; cur = &(*cur)->next_free) {
        if ((*cur)->size < total_size) {
            continue;
        }
        uintptr_t raw = (uintptr_t)(*cur) + sizeof(AllocationHeader);
        uintptr_t aligned = (raw + (alignment - 1)) & ~(alignment - 1);
        AllocationHeader *hdr = (AllocationHeader *)(aligned - sizeof(AllocationHeader));
        hdr->size = (*cur)->size;
        *cur = (*cur)->next_free; /* remove from free list */
        return (void *)aligned;
    }

    /* Allocate from bump buffer. */
    if (allocator_offset + total_size > ALLOCATOR_SIZE) {
        return NULL;
    }

    uintptr_t raw = (uintptr_t)&allocator_buffer[allocator_offset] + sizeof(AllocationHeader);
    uintptr_t aligned = (raw + (alignment - 1)) & ~(alignment - 1);
    AllocationHeader *hdr = (AllocationHeader *)(aligned - sizeof(AllocationHeader));
    hdr->size = total_size;

    allocator_offset += total_size;
    return (void *)aligned;
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
