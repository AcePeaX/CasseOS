#include <stdint.h>
#include "include/kernel/bootinfo.h"

extern void kernel_uefi_entry(void);

__attribute__((section(".bootinfo")))
const kernel_bootinfo_t kernel_bootinfo = {
    .magic = KERNEL_BOOTINFO_MAGIC,
    .uefi_entry = (uint64_t)(uintptr_t)&kernel_uefi_entry,
};
