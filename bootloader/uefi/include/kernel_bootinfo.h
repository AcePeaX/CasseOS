#ifndef CASSEOS_UEFI_KERNEL_BOOTINFO_H
#define CASSEOS_UEFI_KERNEL_BOOTINFO_H

#define KERNEL_BOOTINFO_MAGIC 0x4341535345554546ULL

typedef unsigned long long loader_uint64_t;

typedef struct {
    loader_uint64_t magic;
    loader_uint64_t uefi_entry;
} kernel_bootinfo_t;

#endif /* CASSEOS_UEFI_KERNEL_BOOTINFO_H */
