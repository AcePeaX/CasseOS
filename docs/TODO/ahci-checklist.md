# AHCI Bring-Up Checklist (from OSDev Wiki PDF)

Reference material: `docs/drivers/block/ahci/AHCI - OSDev Wiki.pdf`.

## Controller Ownership & PCI Setup
1. Scan PCI for class `0x01`, subclass `0x06`, prog-if `0x01` devices.
2. Enable IO space, memory space, and bus mastering bits in the PCI command register.
3. Map BAR5 (ABAR) as uncached MMIO; confirm a non-zero MMIO base.
4. Toggle `GHC.AE` (AHCI enable) and execute the BIOS/OS handoff sequence described in `CAP2`/`BOHC` if firmware still owns the controller.
5. Register the controller’s IRQ line (even if you poll initially) since AHCI lines are usually shared.

## Port Discovery & Classification
1. Read `HBA_MEM.PI` to know which ports exist.
2. For each implemented port:
   - Read `PxSSTS`; only continue if `DET == 3` (device present) and `IPM == 1` (active).
   - Read `PxSIG` to differentiate SATA, SATAPI, SEMB, or Port Multiplier (`0x00000101`, `0xEB140101`, etc.).
   - Cache this metadata for shell diagnostics (`probe_port()` equivalent).

## Port Rebase Procedure
1. Stop the port: clear `PxCMD.ST` and `PxCMD.FRE`, then wait for `PxCMD.CR/F R` to drop to zero.
2. Allocate aligned buffers per OSDev guidance:
   - Command List: 1 KiB aligned.
   - Received FIS: 256 B aligned.
   - Command Tables: 128 B aligned (one per slot, sized for CFIS + ACMD + PRDT array).
   - PRDT entries (at least one per command; data buffers must be physically contiguous).
3. Program `PxCLB/CLBU` and `PxFB/FBU` with the new bases; zero the memory regions.
4. For each command header: set `prdtl`, point `CTBA/CTBAU` to the matching command table, and clear the structure.
5. Start the port: set `PxCMD.FRE`, then `PxCMD.ST`, verifying `PxCMD.CR` remains clear.

## Command Submission Flow
1. Slot allocation: examine `PxSACT | PxCI`; a zero bit indicates a free slot (`find_cmdslot()` pattern).
2. CFIS construction:
   - Use FIS type `0x27`, set the command bit, command code, LBA fields (48-bit for READ DMA EXT), sector counts, and device bit 6 for LBA mode.
3. PRDT construction:
   - Each entry holds a data buffer (address + byte count minus one). Set the IOC bit if you rely on interrupts.
   - For IDENTIFY, a single 512 B buffer works; for reads, allocate `(sector_count * 512)` bytes.
4. Clear `PxIS` prior to issuing a command to avoid stale errors.
5. Set the slot bit in `PxCI` to launch the command.

## Polling & Error Handling
1. Spin on `PxCI` bit until it clears. Optionally watch `PxIS` DPS/TFES bits for quicker failure detection.
2. Also poll `PxTFD`: abort if `ATA_DEV_BUSY` or `ATA_DEV_DRQ` stay high too long (per OSDev’s million-iteration guard).
3. On error (`PxIS.TFES`), log `PxTFD`, `PxSERR`, and consider resetting the port via the stop/start sequence.

## IDENTIFY & READ DMA EXT
1. IDENTIFY (0xEC):
   - Does not require an LBA; request 1 sector.
   - Parse words 27–46 for the model string (byte-swapped) and words 60–61 / 100–103 for sector counts.
   - Store this information in the block-device registry for shell commands.
2. READ DMA EXT (0x25):
   - Use the sample code from the wiki (LBA split across lba0-lba5, `countl/counth` for sectors).
   - After completion, verify the target buffer contains the MBR signature (0x55AA) when reading LBA 0 in tests.

## Interrupt Path (Future)
1. Unmask desired bits in `PxIE` (e.g., D2H FIS) and set the global interrupt enable in `GHC`.
2. In the shared IRQ handler:
   - Read `IS`, acknowledge handled bits.
   - For each port flagged in `IS`, read `PxIS`, handle completions/errors, and clear `PxIS`.
   - Compare the `PxCI` bitmap to your outstanding command mask to detect completions.

## ATAPI Considerations
1. For optical drives (signature `0xEB140101`), issue the PACKET command (0xA0) instead of READ DMA.
2. Populate the command table’s `ACMD` region with the 12/16-byte ATAPI command.
3. Set the `ATAPI` flag in the command header to let hardware handle the PACKET flow automatically.

## External Links
- Intel AHCI Spec 1.3
- SATA 3.0 spec
- OSDev AHCI wiki (mirrored in `docs/drivers/block/ahci/AHCI - OSDev Wiki.pdf`)
