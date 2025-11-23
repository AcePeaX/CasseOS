# BIOS vs UEFI AHCI Choice

## Context
- UEFI boots: AHCI works; firmware initializes the HBA and ports, signatures are valid, and IDENTIFY succeeds.
- BIOS boots: AHCI ports report `sig=0xFFFFFFFF` even after COMRESET/power-on; IDENTIFY times out and UHCI USB suffers as a result.
- We still want BIOS boot for legacy coverage, but we need stable USB input there.

## Decision
- Skip AHCI initialization on BIOS boots (detected by missing `kernel_bootinfo.magic`).
- Leave AHCI fully enabled on UEFI boots.

## Rationale
- BIOS firmware leaves AHCI uninitialized; no real SATA signature is reported on the attached virtual disk, causing AHCI command timeouts that wedge USB.
- UEFI firmware pre-inits AHCI, so disabling it there would drop working functionality.
- Skipping AHCI on BIOS avoids timeouts and keeps USB reliable while preserving UEFI AHCI support.

## Consequences
- BIOS boots have no AHCI disk support (USB/PS2 input remains stable).
- UEFI boots keep AHCI enabled and working.
- If BIOS AHCI is needed later, we must supply a proper SATA image and/or a more robust HBA bring-up that yields valid signatures before IDENTIFY.
