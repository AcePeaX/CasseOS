# AHCI Page Fault Debug Notes

## Symptom
- Booting the BIOS path (not UEFI) crashed immediately after `irq_install()` when we added the block device registry AHCI scaffolding.
- GDB showed an interrupt 14 (page fault) pointing into `drivers/block/ahci/controller.c`, specifically at `ctrl->implemented_ports = ctrl->hba_mem->pi;`.
- UEFI boots worked fine, only BIOS builds faulted.

## Root Cause
- The legacy BIOS entry path sets up identity paging for only the first 2 MiB of physical memory in `kernel/kernel-long-mode-transition.asm`. That was sufficient before AHCI read the controller’s MMIO registers.
- Under BIOS, the AHCI controller’s BAR5 (ABAR) lives above 2 MiB (e.g., at 0xFEC1xxxx). Accessing `ctrl->hba_mem->pi` dereferenced that high address while paging only mapped 0–2 MiB -> CPU raised a page fault.
- UEFI firmware already configured long mode with large identity-mapped ranges, so UEFI builds didn’t need to expand paging.

## Fix
- Rewrote the BIOS paging setup (still in `kernel/kernel-long-mode-transition.asm`) to map 0–4 GiB using 2 MiB pages across four page-directory tables.
- New map macros set up PML4 → PDPT → four PDs with `PS` (page size) bit, yielding contiguous identity mapping for low RAM and high MMIO.
- With those entries in place, BIOS builds can safely touch AHCI/PCI MMIO regions, and the page fault disappears.

## Lessons Learned
- Any MMIO access above the initial 2 MiB needs matching page-table coverage; the BIOS path had to mirror what UEFI implicitly provides.
- When faults differ between BIOS and UEFI, check assumptions about firmware-provided paging vs. our hand-rolled tables.

