#ifndef CASSEOS_KERNEL_BOOTINFO_H
#define CASSEOS_KERNEL_BOOTINFO_H

#include <stdint.h>

#define KERNEL_BOOTINFO_MAGIC 0x4341535345554546ULL /* "CASSEUEF" */

typedef struct {
    uint64_t magic;
    uint64_t uefi_entry;
    uint64_t flags;
} kernel_bootinfo_t;

#define KERNEL_BOOTINFO_FLAG_UEFI 0x1

#endif /* CASSEOS_KERNEL_BOOTINFO_H */
