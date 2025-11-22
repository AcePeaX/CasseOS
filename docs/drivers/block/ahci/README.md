# AHCI Overview (Beginner → Advanced)

AHCI (Advanced Host Controller Interface) is the standard way an OS talks to SATA controllers in “AHCI mode.” It defines how to discover controllers on PCI, map their MMIO registers, enumerate ports, and send commands (IDENTIFY, READ/WRITE DMA, etc.) using FIS structures and DMA descriptors.

## 1) Core Concepts (Beginner)
- **Controller & Ports:** A single PCI AHCI controller exposes up to 32 ports. Each port can attach a SATA device (disk, SSD, ATAPI).
- **MMIO Registers:** The controller exposes an MMIO BAR (ABAR). Inside ABAR is the HBA register block (capabilities, global control, interrupt status) and a per-port register block.
- **DMA, not PIO:** Commands are issued with command lists + command tables; data moves via PRDT entries (scatter/gather DMA). The CPU does not PIO data words.
- **FIS (Frame Information Structure):** Packets exchanged between host and device. We build Host-to-Device (Reg H2D) FIS to issue commands like IDENTIFY or READ DMA.

## 2) Discovery & Mapping (Intermediate)
- **PCI match:** Class 0x01, subclass 0x06, prog-if 0x01. Read BAR5 (typical ABAR), require MMIO.
- **Map ABAR:** The HBA register block starts at ABAR. Key top-level registers: `CAP`, `GHC`, `IS`, `PI`, `VS`, `CAP2`, `BOHC`.
- **Ports present:** `PI` bitmask tells which ports are implemented. Each implemented port has its own register block at `ports[n]`.
- **Device presence:** Per-port `SSTS` (DET/SPEED/IPM) shows if a SATA link is up. `SIG` gives a device signature (SATA vs SATAPI vs PM).

## 3) Port Bring-Up (Intermediate)
- **Stop the port:** Clear `PxCMD.ST` and `PxCMD.FRE`, then wait for `PxCMD.CR`/`FR` to clear.
- **Allocate buffers:** Per port you need:
  - Command list (32 entries, 1 KiB aligned)
  - Received FIS buffer (256 bytes aligned)
  - Command table(s) with PRDTs (128-byte aligned; size depends on PRDT count)
  - Scratch DMA buffer for data (aligned to cacheline/page; sized for transfer)
- **Program bases:** Write `PxCLB/CLBU` with the command list base, `PxFB/FBU` with the received FIS buffer.
- **Start the port:** Set `PxCMD.FRE`, then `PxCMD.ST`.

## 4) Issuing a Command (Intermediate/Advanced)
- **Choose a slot:** Find a free command slot (bit in `PxCI` clear).
- **Build Command Header:** Set CFIS length (DWORD count), write flag (W), PRDT length, CTBA/CTBAU pointing to the command table.
- **Build Command Table:** Zero it, fill PRDT entries with physical addresses + byte counts (DBC includes byte_count-1, set IOC bit for interrupt/end).
- **Build CFIS:** Use Reg H2D (type 0x27). Set `C=1` (command). Fill LBA (48-bit) and sector count for READ/WRITE DMA EXT (0x25/0x35) or IDENTIFY (0xEC, no LBA). Set device bit 6 for LBA mode.
- **Kick it:** Clear port interrupt status, then set the slot bit in `PxCI`.
- **Wait:** Poll `PxCI` (or use interrupts) until the slot bit clears. Watch `PxIS.TFES` for taskfile errors; also ensure `PxTFD.BSY/DRQ` clear.

## 5) IDENTIFY & Geometry (Intermediate)
- Send IDENTIFY DEVICE (0xEC) to SATA disks. The 512-byte response includes:
  - Model string (words 27–46, byte-swapped per word)
  - 28-bit LBA sectors (words 60–61)
  - 48-bit LBA sectors (words 100–103) when supported
- Use the 48-bit value when non-zero; fall back to 28-bit otherwise.

## 6) READ/WRITE DMA EXT (Advanced)
- Command code 0x25 (read) or 0x35 (write) with 48-bit LBA + sector count.
- PRDT can describe multiple segments; for simplicity start with one PRDT covering your DMA buffer sized to `sector_count * 512`.
- Sector size is 512 bytes for legacy disks; 4K-native drives exist but are rare in virtual setups like QEMU.

## 7) Interrupts vs Polling (Advanced)
- **Polling path:** Clear `PxIS`, set `PxCI`, spin on `PxCI` bit and `PxIS` error flags.
- **Interrupt path:** Unmask `PxIE`, handle port interrupts in ISR, acknowledge `PxIS`/`IS`. Requires routing PCI INTx/MSI and registering an ISR for the AHCI controller’s IRQ.

## 8) BIOS/OS Handoff & Reset (Advanced)
- Some firmware leaves ports running. Safest: stop ports (clear ST/FRE), optionally perform COMRESET via `PxSCTL.DET`, then restart.
- `BOHC` can coordinate BIOS/OS ownership; QEMU is usually tolerant, but hardware may need a handshake.

## 9) QEMU Notes (Practical)
- Add an AHCI controller and disk: `-device ahci -drive if=none,id=disk,file=disk.img,format=raw -device ide-hd,drive=disk,bus=ahci0.0`.
- ABAR will be in BAR5, memory-mapped. QEMU reports SATA link up (DET=3) when a disk is attached.
- Useful commands to debug: IDENTIFY (0xEC) then READ DMA EXT (0x25) sector 0.

## 10) Common Pitfalls
- Forgetting to stop/start ports around buffer reprogramming (ST/FRE/CR/FR bits).
- Misaligned command list/FIS/command tables (alignment requirements matter).
- Not clearing `PxIS` before issuing a command; latent errors can cause immediate failures.
- Using 28-bit LBA fields by accident; use 48-bit LBA for modern sizes.
- Overrunning DMA buffer: PRDT DBC is byte_count-1, and you must size buffers for the requested sectors.

## 11) Minimal Bring-Up Checklist (Practical)
1) Discover AHCI PCI device; enable bus mastering; map BAR5 (ABAR).
2) Read `PI`, loop ports with DET=3 and SATA signature.
3) Stop port, allocate + program CLB/FB, start port.
4) Issue IDENTIFY; parse model + 48-bit sector count.
5) Issue READ DMA EXT on a small buffer (e.g., LBA 0, 1 sector) and hex-dump to validate the data path.
6) Register the device in a block layer so higher subsystems (filesystems, shell) can request reads by LBA.

## 12) Next Steps
- Add write support (0x35), multiple PRDTs, and queued slots.
- Enable interrupts (PxIE + PCI IRQ hookup) to avoid polling.
- Layer a filesystem parser (e.g., FAT) on top of the block interface to load binaries from disk.
