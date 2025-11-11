#include "type.h"   

static inline uint64_t read_cr0(void) {
    uint64_t val;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(val));
    return val;
}

static inline uint64_t read_cr4(void) {
    uint64_t val;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(val));
    return val;
}

static inline void write_cr0(uint64_t val) {
    __asm__ volatile ("mov %0, %%cr0" :: "r"(val) : "memory");
}

static inline void write_cr4(uint64_t val) {
    __asm__ volatile ("mov %0, %%cr4" :: "r"(val) : "memory");
}

void cpu_enable_fpu_sse(void) {
    uint64_t cr0 = read_cr0();
    uint64_t cr4 = read_cr4();

    // Enable FPU and SSE
    cr0 &= ~(1ULL << 2); // EM = 0 (no emulation)
    cr0 |=  (1ULL << 1); // MP = 1 (monitor coprocessor)
    cr0 &= ~(1ULL << 3); // TS = 0 (task-switched off for now)

    cr4 |= (1ULL << 9);  // OSFXSR = 1 (FXSAVE/FXRSTOR support)
    cr4 |= (1ULL << 10); // OSXMMEXCPT = 1 (SSE exceptions)

    write_cr0(cr0);
    write_cr4(cr4);

    __asm__ volatile ("fninit"); // Initialize FPU/x87 state
}
