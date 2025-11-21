# UEFI IRQ + Keyboard Notes

## Background
- `irq_install()` remaps the legacy 8259 PIC, programs the PIT, and finishes by executing `sti`.
- Under BIOS boot this works because the platform still routes hardware interrupts through the 8259; we rely on IRQ1 for PS/2 keyboard input and IRQ0 for the PIT tick.
- Under UEFI/OVMF the firmware leaves long mode with the IOAPIC enabled and the legacy PIC masked. After `ExitBootServices`, enabling `irq_install()` causes spurious interrupts that we neither acknowledge correctly nor have handlers for after the remap, which leads to faults and a reboot loop.

## Observed Failure
- When we call `irq_install()` on the UEFI path the kernel crashes almost immediately.
- The crash happens before any input/output because the PIT interrupt starts firing but the interrupt is not fully serviced: either because the IRQ vectors (32+) are unmapped on the IOAPIC or because the legacy PIC we remap at 0x20/0xA0 never sees the acknowledge sequence.
- Result: triple fault/reset before the shell comes up, so the keyboard never works.

## Likely Cause
1. **Interrupt controller mismatch**: On UEFI the IOAPIC delivers interrupts, but our ISR code still expects the 8259. Writing to ports `0x20/0xA0` does nothing, so IRQ0 keeps firing endlessly. With `sti` set we re-enter the same interrupt, push garbage on the stack, and crash.
2. **Timer IRQ storms**: `irq_install()` programs the PIT to fire at 4 kHz. While VGA path handles the interrupt, on UEFI the handler never clears the IOAPIC bit, so we trigger nested interrupts and blow the stack.
3. **Keyboard hardware**: Even if the IOAPIC issue were solved, many UEFI machines only expose USB keyboards; we would still get no input because our USB HID stack doesn’t feed events into `kbd_dispatch_event()`.

## Short-Term Options
1. **Keep IRQs masked on UEFI**: current workaround; rely on polling/serial for early bring-up. This keeps the kernel stable but offers no PS/2 keyboard support.
2. **Polling driver**: add a simple PS/2 polling loop that reads port `0x60` periodically when IRQ1 isn’t available. This would allow PS/2 keyboards to work even with interrupts disabled.
3. **USB keyboard path**: finish the UHCI keyboard pipeline so that `usb_hid_mapper` feeds `keyboard_common`. USB still requires interrupts or periodic polling of the controller.

## Long-Term Fix
- Implement proper IOAPIC/LAPIC initialization after `ExitBootServices`: map the IOAPIC registers, route vectors 32–47 to our IRQ stubs, and send EOIs via the LAPIC instead of the 8259 ports.
- Once IOAPIC routing is working we can safely enable `irq_install()` (or similar) on both BIOS and UEFI paths.
- In parallel, complete USB HID keyboard handling so we aren’t limited to legacy PS/2 hardware.

## Next Steps
1. Prototype a PS/2 port polling helper for UEFI builds so the shell can receive input without IRQ1.
2. Investigate IOAPIC register state after `ExitBootServices` (dump via MMIO) to verify the exact failure mode when enabling interrupts.
3. When ready, introduce a proper APIC abstraction so BIOS can keep using the 8259 while UEFI switches to IOAPIC/LAPIC, after which we can re-enable `irq_install()` universally.
