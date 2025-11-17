#ifndef CASSEOS_UEFI_KERNEL_BOOTINFO_H
#define CASSEOS_UEFI_KERNEL_BOOTINFO_H

#define KERNEL_BOOTINFO_MAGIC 0x4341535345554546ULL
#define KERNEL_BOOTINFO_FLAG_UEFI 0x1
#define KERNEL_BOOTINFO_FLAG_FRAMEBUFFER 0x2

typedef unsigned long long loader_uint64_t;
typedef unsigned int loader_uint32_t;

typedef struct {
    loader_uint64_t magic;
    loader_uint64_t uefi_entry;
    loader_uint64_t flags;
    loader_uint64_t fb_base;
    loader_uint64_t fb_size;
    loader_uint32_t fb_width;
    loader_uint32_t fb_height;
    loader_uint32_t fb_stride;
    loader_uint32_t fb_bpp;
} kernel_bootinfo_t;

#endif /* CASSEOS_UEFI_KERNEL_BOOTINFO_H */
