#ifndef CASSEOS_KERNEL_BOOTINFO_H
#define CASSEOS_KERNEL_BOOTINFO_H

#include <stdint.h>

#define KERNEL_BOOTINFO_MAGIC 0x4341535345554546ULL /* "CASSEUEF" */

typedef struct {
    uint64_t magic;
    uint64_t uefi_entry;
    uint64_t flags;
    uint64_t fb_base;
    uint64_t fb_size;
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_stride;
    uint32_t fb_bpp;
} kernel_bootinfo_t;

#define KERNEL_BOOTINFO_FLAG_UEFI 0x1
#define KERNEL_BOOTINFO_FLAG_FRAMEBUFFER 0x2

#endif /* CASSEOS_KERNEL_BOOTINFO_H */
