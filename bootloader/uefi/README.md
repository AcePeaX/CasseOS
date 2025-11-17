# UEFI bootloader stub

This directory is reserved for the upcoming native UEFI loader. The BIOS implementation lives in `bootloader/bios/` and continues to be built into `bootloader.bin` while we bring up the new PE/COFF entry point.

The current implementation builds a minimal PE/COFF application that UEFI firmware can launch from `EFI/BOOT/BOOTX64.EFI`. It simply writes `Hello bootloader` through the firmware console so we can validate the build chain and disk layout in QEMU/OVMF. Once the native loader grows features, this will be the entry point that loads the shared kernel image from the FAT32 ESP.
