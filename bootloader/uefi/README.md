# UEFI bootloader stub

This directory is reserved for the upcoming native UEFI loader. The BIOS implementation lives in `bootloader/bios/` and continues to be built into `bootloader.bin` while we bring up the new PE/COFF entry point.

The current implementation builds a PE/COFF application that the firmware launches from `EFI/BOOT/BOOTX64.EFI`. It mounts the ESP, reads `\CASSEKRN.BIN`, copies it to physical address `0x80000` (matching the BIOS loader), validates the embedded boot-info header to discover the 64-bit UEFI entry point inside the kernel image, allocates a stack, exits boot services, and jumps into the kernel. The kernel now exposes a dedicated `kernel_uefi_entry` label so both firmware paths reuse the same binary.
