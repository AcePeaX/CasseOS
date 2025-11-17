# UEFI bootloader stub

This directory is reserved for the upcoming native UEFI loader. The BIOS implementation lives in `bootloader/bios/` and continues to be built into `bootloader.bin` while we bring up the new PE/COFF entry point.

For now the FAT32 partition that the image builder produces just contains a placeholder `BOOTX64.EFI` file generated automatically by `scripts/build_disk_image.sh`. The file is a two-byte infinite loop so that, once we start emitting a real PE/COFF binary, we can replace the stub without touching the disk-layout logic.
