# BIOS + UEFI Boot Compatibility Plan

This plan makes CasseOS bootable on machines that expose either legacy BIOS or modern UEFI firmware without requiring the user to rebuild different images. The goal is a single disk image that contains the existing BIOS loader path plus a native UEFI application that jumps into the same 64-bit kernel entry point.

## 1. High-Level Objectives
- Keep today’s BIOS boot flow working so existing `os-image.bin` users see no regression.
- Add a UEFI loader that enters long mode on its own (no CSM) and hands off to the kernel with the same boot info structure as the BIOS path.
- Produce one disk artifact (raw image + optional ISO) that embeds both boot methods so hypervisors and hardware can pick whichever firmware interface they offer.
- Document and test both flows so regressions are caught early (CI smoke test with QEMU/OVMF later).

## 2. Disk / Image Layout
1. **Protective MBR + stage-1 boot sector**
   - First 512 B retains BIOS stage-1 but also carries a valid MBR partition table (type 0xEE protective entry) so GPT-aware firmware/OSes leave the disk alone.
   - Stage-1 only job: load a stage-2 binary via BIOS INT 13h LBA (no more manual sector counts).
2. **GPT**
   - Create GPT with at least two partitions:
     - `p1` – small BIOS boot partition (e.g., 1 MiB) containing the stage-2 loader image.
     - `p2` – FAT32 ESP (at least 64 MiB) that stores `/EFI/BOOT/BOOTX64.EFI`, the kernel binary, and optional initramfs.
   - Optionally keep a raw area after ESP for other files but GPT gives flexibility for future modules.
3. **Shared kernel binary**
   - Kernel gets written once (e.g., `/EFI/BOOT/CASSEKRN.BIN`) and both loaders read from there so there’s no duplication.

## 3. BIOS Boot Flow Upgrades
1. **Stage-1 (512 B, bootloader/legacy/stage1.asm)**
   - Enable A20 (fast gate) before jumping to stage-2.
   - Use BIOS extension `int 0x13 ah=0x42` to fetch stage-2 via LBA from the BIOS boot partition; stage-2 size gets encoded in BPB or a tiny metadata block at LBA2.
2. **Stage-2 (bootloader/legacy/stage2.asm + helpers)**
   - Relocate to 0x00080000 as today but split responsibilities:
     - Switch to unreal/protected mode to copy remaining kernel chunks >1 MiB if needed.
     - Reuse existing `32bit-*` helpers for GDT setup; extend to build the final long-mode page tables.
   - Enumerate memory map via `int 0x15 e820`.
   - Load the kernel file from disk using LBA + FS driver:
     - Minimal read-only FAT32 reader (cluster walk) stored inside stage-2 to pull `/EFI/BOOT/CASSEKRN.BIN`.
     - Alternative short-term approach: reserve contiguous sectors for kernel and keep metadata table; switch to FAT reader later.
   - Construct a `boot_info` struct (memory map, framebuffer placeholder, boot mode flag) and jump to kernel entry point in long mode.
3. **Deliverables**
   - `bootloader/legacy/` directory with stage1/2 + shared NASM helpers.
   - Updated Makefile rule to produce `bios-stage2.bin` and bundle with disk image builder script.

## 4. UEFI Boot Flow
1. **Loader implementation**
   - New target `bootloader/uefi/main.c` compiled with `x86_64-elf-gcc` using `-ffreestanding -fno-stack-protector -fshort-wchar` etc., linked with `lld`/`gnu-efi` headers manually (no external deps beyond repo-provided headers).
   - Output PE/COFF image `BOOTX64.EFI`.
2. **Responsibilities**
   - Use UEFI Simple File System to open the ESP and read the same kernel binary as BIOS stage-2 (`\EFI\BOOT\CASSEKRN.BIN`).
   - Allocate page-aligned memory below 4 GiB for compatibility, copy kernel, and gather:
     - Memory map
     - ACPI RSDP pointer
     - Framebuffer mode info via GOP
     - Command line/config file (optional).
   - ExitBootServices, enable paging/long mode (if not already), set up stack, and branch to kernel entry with the `boot_info` structure.
3. **Build orchestration**
   - Add a `uefi` make target that produces the `.efi` binary and copies it into `.bin/esp/EFI/BOOT/`.
   - Use `scripts/gen-disk-image.py` (new) to:
     - Create raw disk file.
     - Partition via `sgdisk`/`parted` or Python `pyfdisk` (no extra host tools ideally).
     - Format ESP with `mtools` or `fatfs` helper and copy files.

## 5. Kernel Handoff Contract
To keep both loaders in sync, define a single `struct boot_info` (placed in `kernel/include/boot/boot_info.h`) and require both loaders to populate it.

Fields (initial draft):
```
struct boot_info {
    uint32_t magic;          // 'CASS'
    uint32_t version;        // struct layout version
    uint64_t kernel_phys_base;
    uint64_t kernel_entry;
    uint64_t stack_top;
    uint32_t boot_mode;      // 0 = BIOS, 1 = UEFI
    uint32_t framebuffer_type;
    struct framebuffer fb;
    struct memory_map map;
    struct rsdp_descriptor *rsdp;
};
```
- Create shared header for loaders + kernel.
- Kernel validates `magic/version` before using data.

## 6. Implementation Phases
1. **Phase A – Repo prep**
   - Restructure `bootloader/` into `legacy/` and `uefi/`.
   - Add `include/boot/` headers and shared assembly helpers.
   - Introduce Python script that builds disk image (works in current toolchain).
2. **Phase B – BIOS modernization**
   - Implement INT 13h extensions + stage-2 loader.
   - Replace hard-coded sector counts with metadata block.
   - Validate big kernel loads (>1 MiB) and memory map passing.
3. **Phase C – UEFI loader**
   - Bring up minimal PE/COFF loader, hook into GOP, exit boot services, jump to kernel.
   - Share page-table creation logic between BIOS and UEFI via a small C library compiled twice (or hand-written asm).
4. **Phase D – Unified artifact**
   - Generate combined disk image, test with QEMU:
     - BIOS: `qemu-system-x86_64 -drive format=raw,file=.bin/casseos.img`
     - UEFI: `qemu-system-x86_64 -bios OVMF.fd -drive ...`
   - Optionally produce an El Torito ISO with both legacy and EFI boot entries.
5. **Phase E – Automation & docs**
   - Update `README.md` with usage instructions.
   - Add smoke-test script that boots both modes in CI and checks for kernel banner via serial console.

## 7. Open Questions / Follow-Ups
- Decide whether the kernel stays in the disk root or under `/EFI/BOOT/`; mirroring it twice simplifies BIOS for now but wastes space.
- Determine if we need GRUB compatibility later; current plan keeps everything custom.
- Long-term: consider making the UEFI loader capable of loading modules or initrd so the BIOS path can piggyback by simply calling into it after switching to long mode.

Following this plan will give CasseOS a first-class, firmware-agnostic boot story while keeping the current hand-written BIOS loader maintainable.
