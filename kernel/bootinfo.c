#include <stdint.h>
#include "kernel/include/kernel/bootinfo.h"

extern void kernel_uefi_entry(void);
extern uint64_t kernel_boot_flags;

__attribute__((section(".bootinfo")))
const kernel_bootinfo_t kernel_bootinfo = {
    .magic = KERNEL_BOOTINFO_MAGIC,
    .uefi_entry = (uint64_t)(uintptr_t)&kernel_uefi_entry,
    .flags = 0,
};
