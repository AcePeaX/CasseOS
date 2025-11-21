# Kernel Shell Rework

These notes capture why the current shell lives inside the kernel and how we can evolve it into a better-defined subsystem (and eventually a user-space program).

## Current Layout
- Source lives under `kernel/shell/` with `shell_main_loop()` (`kernel/shell/shell.c`) polled from the `while(true)` loop in `kernel/kernel.c`.
- The same loop also scans PCI/USB devices and initializes drivers, so the shell effectively *is* the kernel's idle loop.
- Keyboard input flows straight from `kbd_read_event()` into `handle_command_line()`, and commands are compared against hard-coded strings inside the shell module.
- Output is sent directly to `kprint()`/`drivers/screen`, so the shell cannot target other outputs (serial console, framebuffer console, etc.) without editing kernel code.

## Why This Is A Problem
- **No separation of concerns:** Command parsing, driver I/O, and kernel orchestration all live in the same loop, making it hard to reason about faults or extend features.
- **Hard to extend commands:** Subsystems cannot register their own commands; everything must be edited into `shell_main_loop()`, creating merge conflicts and tight coupling.
- **Blocks future scheduling:** Because the shell runs in the kernel loop, we cannot easily introduce timers, background work, or cooperative tasks without teaching the shell about them.
- **Assumes one TTY backend:** Direct `kprint()` usage means we cannot reuse the shell over serial or the framebuffer console initialized in `framebuffer_console_init()`.

## Short-Term Refactor Plan
1. **Split front-end vs. command core.** Keep a lightweight shell core that only manages a command table and command-line editing. Move keyboard and cursor logic into a `tty_keyboard` helper that feeds characters into the shell.
2. **Introduce a command registry.** Expose `shell_register_command(name, handler, help)` so PCI/USB/etc. can register commands at init time without modifying the shell file.
3. **Abstract console I/O.** Define a small interface (`struct shell_io { write, read_key, cursor_ops }`) backed by screen, framebuffer console, or serial. This lets us switch outputs and reuse the shell over different transports.
4. **Make the kernel loop scheduler-friendly.** Convert the busy `while(true) shell_main_loop();` into something that lets the shell yield—e.g., poll from the PIT interrupt handler or an explicit `shell_poll()` called from an idle task stub.

## Long-Term Direction
- Once tasking and memory protection land, promote the shell to the first user task. The kernel should expose syscalls for console I/O and command registration (or IPC), letting the shell become an ordinary program.
- Keep the kernel-resident shell core minimal so we can delete it once user space is ready; only the command registry shim should remain for debugging builds.

## Next Steps
1. Write a design doc for the command registry API (headers, init order, ownership of buffers).
2. Implement the console abstraction under `drivers/screen/` so both the boot-time screen and the framebuffer console can be targets.
3. Update `kernel/kernel.c` to call into the new scheduler/idle task entry instead of spinning exclusively on the shell.
4. Port existing commands (`usb_scan`, etc.) to the registration API and delete the current hard-coded string comparisons.
