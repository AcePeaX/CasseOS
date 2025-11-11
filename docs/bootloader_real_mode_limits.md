# Real-Mode Loading Constraints & Next Steps

The current stage-1 bootloader (`bootloader/bootloader.asm`) stays in BIOS real mode and uses the legacy `int 0x13` CHS call to copy the kernel image into memory at `0x80000`. That works for the present ~32 KiB payload, but it imposes strict limits you’ll hit as soon as the kernel and its libraries grow.

## 1. 64 KiB buffer window per BIOS read
`disk_load` places the BIOS transfer buffer at `ES:BX = 0x8000:0`. Because offsets are only 16 bits, the BIOS write pointer wraps after 64 KiB. Any single `int 0x13` read larger than that trashes earlier data.

**Implication:** you can’t load arbitrarily large binaries in one shot while staying in 16-bit mode. Either:
- read in ≤64 KiB chunks and manually bump `ES` between reads, or
- switch to protected/unreal/long mode before continuing the load.

## 2. 1 MiB physical ceiling before enabling A20/switching modes
Real-mode addressing maxes out just shy of 1 MiB. With the kernel placed at `0x80000`, you only have ~0x80000 bytes (512 KiB) before colliding with the 1 MiB barrier or ROM/EBDA/VGA regions. That space must hold the kernel *plus* any early libraries (memory manager, disk I/O, etc.).

**Implication:** as soon as your kernel + early libs exceed ~480 KiB, you need a loader that enables A20 and switches to protected mode (or long mode) before pulling in the remaining image.

## 3. CHS INT 13h addressing limits
The current call uses Cylinder/Head/Sector addressing (AH=0x02). It caps you at 63 sectors per request and 1024 cylinders overall. Modern BIOSes offer extended INT 13h (AH=0x42) with LBA support, but you need to implement that explicitly if you keep loading in real mode.

---

## Roadmap: Disk driver for long mode
To break out of these limits you eventually need a two-stage strategy:

1. **Minimal real-mode stage (current boot sector + small loader)**
   - Only responsibility: load a *bootstrap* portion of the kernel that includes the A20 enable, GDT, paging setup, and a basic disk/LBA driver.
   - Keep this payload well under the 512 KiB space at 0x80000 (ideally <128 KiB) so it fits within the real-mode window.

2. **Early protected/long-mode loader**
   - Once paging and long mode are enabled, use 32/64-bit disk routines (PIO or AHCI) or BIOS extended reads to fetch the remainder of the kernel (higher-half text, drivers, modules). At this point you’re no longer limited by 64 KiB segments or the 1 MiB ceiling.

## Binary layout recommendation
Order the kernel binary so that the very beginning contains everything the early loader needs before it can continue loading in long mode:

1. **Stage 2 / bootstrap code**
   - Paging/A20 setup
   - Minimal memory map parser (e820)
   - Basic disk driver capable of reading the rest of the image (IDE/ATA PIO or AHCI, or at least BIOS extended INT 13h fallback)

2. **Essential runtime libs** required by that bootstrap (string/mem routines, logging, maybe basic heap if the loader uses it).

3. **Remaining kernel sections** (higher-half text, subsystems, full drivers).

When linking, place the bootstrap objects first so they reside in the low offsets that the real-mode loader can reach. Everything after that can live anywhere because the long-mode loader will fetch it after switching modes.

**Summary:** stay under ~64 KiB per read and ~480 KiB total while in real mode, but start planning a staged load: a small bootstrap that gets you into long mode plus disk drivers, followed by the rest of the kernel once those drivers are active. Organize the binary so the bootstrap and its dependencies come first.
