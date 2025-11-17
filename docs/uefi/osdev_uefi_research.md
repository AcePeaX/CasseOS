# OSDev UEFI research notes

Source: [OSDev Wiki – UEFI](https://wiki.osdev.org/UEFI) (snapshot stored in `docs/osdev_uefi.html`).

## UEFI vs. legacy BIOS
- **Firmware responsibilities:** BIOS leaves you in 16-bit real mode where the loader must enable A20, set up a GDT/IDT, then transition to protected/long mode. UEFI already enables A20 and boots you directly into 32-bit flat protected mode or 64-bit long mode with identity paging.
- **Boot contract:** BIOS executes the 512-byte MBR boot sector at 0x7C00, whereas UEFI loads a PE/COFF executable (`BOOTX64.EFI`) of arbitrary size from a FAT partition (on GPT or MBR) and calls its entry point.
- **Discovery:** BIOS loaders must manually scan EBDA/SMBIOS/ACPI/etc. UEFI hands a system table pointer to the loader that already exposes ACPI pointers, the memory map, and firmware protocols.
- **APIs:** BIOS exposes ad-hoc `int` handlers (e.g., INT 13h) with non-uniform calling conventions. UEFI offers structured "protocols" (Simple File System, Graphics Output, etc.) callable via a modern ABI, and applications can publish their own protocols.
- **Tooling:** BIOS loaders are raw binaries built with NASM/GCC. UEFI apps are PE executables that can be produced with EDK2, GNU-EFI, or POSIX-UEFI style build flows; we plan to keep using our cross GCC/LD toolchain to emit PE via GNU-EFI-style stubs.

## Emulation with QEMU and OVMF
- Use any recent `qemu-system-x86_64` plus the open OVMF firmware (`OVMF_CODE.fd` + `OVMF_VARS.fd`) from TianoCore.
- Recommended invocation (graphics enabled):
  ```
  qemu-system-x86_64 -cpu qemu64 \
    -drive if=pflash,format=raw,unit=0,file=path_to_OVMF_CODE.fd,readonly=on \
    -drive if=pflash,format=raw,unit=1,file=path_to_OVMF_VARS.fd \
    -drive format=raw,file=.bin/casseos.img \
    -net none
  ```
  Add `-nographic` for headless use.
- If no ESP with a properly named EFI binary is found, OVMF drops into its UEFI shell; `help` lists available commands. This is handy for manual testing while we bring up the loader.

## Creating disk images
- UEFI firmware expects FAT12/16/32 on GPT or MBR, but FAT32 has the widest firmware compatibility; minimum FAT32 partition size is 33,548,800 bytes, so the wiki example uses a 48 MB image (93,750 sectors).
- Disk creation flow:
  1. `dd if=/dev/zero of=uefi.img bs=512 count=93750`
  2. Partition with GPT (`gdisk`) and create an EFI System Partition.
  3. Attach with `losetup`, format with `mkdosfs -F32`, mount, copy `EFI/BOOT/BOOTX64.EFI` plus kernel files, then detach.
- For rapid iteration you can instead use the `uefi-run` helper (`cargo install uefi-run`). It auto-creates a temporary FAT image, injects your `.efi`, and boots QEMU/OVMF with optional extra args: `uefi-run -b OVMF.fd -q qemu-system-x86_64 BOOTX64.EFI -- -net none`.
- Whichever approach we use, our final image builder must ensure the ESP contains `/EFI/BOOT/BOOTX64.EFI` plus `CASSEKRN.BIN`, and optionally include a BIOS boot partition so one disk works for both firmware paths.

## Action items for CasseOS
- Keep building the BIOS loader from `bootloader/bios/` while scaffolding the PE/COFF loader under `bootloader/uefi/`.
- Prepare the build system/scripts to:
  1. Build the `.efi` binary (targeting x86_64, freestanding, `-fshort-wchar` etc.).
  2. Create a GPT disk with a BIOS boot area plus a FAT32 ESP (~64 MB).
  3. Copy shared kernel artifacts into the ESP so both firmware paths load the same binary.
- Add QEMU recipes (`make qemu-uefi` later) that launch with OVMF `-pflash` drives so both BIOS and UEFI boot flows are testable locally and in CI.
