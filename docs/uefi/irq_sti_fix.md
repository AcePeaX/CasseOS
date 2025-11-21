# UEFI Interrupt Enable Notes

Background:
- The BIOS boot flow jumps into `kernel_entry.asm` after the loader already installs the kernel’s GDT.
- The new UEFI path jumped straight into `kernel_uefi_entry` with firmware’s GDT still loaded.

Problem:
- Our IDT entries use selector `0x08` (the kernel code descriptor in our GDT).
- When `irq_install()` issued `sti`, the timer IRQ returned through an `iretq` that referenced selector `0x08`, but this selector was undefined in the firmware GDT.
- Result: immediate vector 13 (General Protection Fault) and watchdog reset.

Fix:
- Extend `gdt_64_descriptor` to store a 64-bit base so it can be loaded from long mode.
- In `kernel_uefi_entry`, load the shared GDT, reload all segment registers, and perform a far return (`retfq`) to refresh CS before touching interrupts.
- With our descriptors active, `irq_install()` can safely execute for both BIOS and UEFI boots.

Verification:
- Boot via `make qemu-uefi`; shell runs, timer ticks, and no vector 13 is emitted when IRQs are enabled.
