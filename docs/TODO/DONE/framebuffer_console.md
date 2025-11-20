# Framebuffer Console (DONE)

## Goals
- Use GOP framebuffer when booted via UEFI.
- Retain VGA text fallback for BIOS.
- Embed a simple bitmap font for early output.

## Completed Work
1. [x] **Boot Info Extensions**
   - Add framebuffer fields (base, size, width, height, stride, bpp) plus a `FRAMEBUFFER_PRESENT` flag to `kernel/include/kernel/bootinfo.h`.
   - Update the loader’s boot-info header to match.
2. [x] **Loader Updates**
   - Locate GOP via `bs->LocateProtocol`.
   - Record framebuffer metadata and set the framebuffer flag before `ExitBootServices`.
3. [x] **Kernel Detection**
   - Read the new boot-info flag.
   - If framebuffer present, initialize the framebuffer console; otherwise keep using VGA text.
4. [x] **Bitmap Font**
   - Embed an 8×16 public-domain font as a static array (`font[256][16]`).
   - Provide a helper to fetch glyph bitmaps by ASCII code.
5. [x] **Blitting Routine**
   - Added `framebuffer_console_draw_glyph` to blit 8×16 glyphs with caller-provided foreground/background colors.
   - Drawing routine enforces bounds and only runs on 32-bit GOP modes.
6. [x] **Console API / Grid**
   - `drivers/screen` now manages a framebuffer backend with 9-pixel columns (8px glyph + 1px padding) and 16-pixel rows.
   - Cursor tracking, newline handling, scrolling, and clearing work for both VGA and framebuffer paths.
7. [x] **Integration**
   - `kprint`, `clear_screen`, and offset helpers automatically dispatch to VGA or framebuffer based on availability.
   - BIOS still uses VGA text I/O; UEFI switches to GOP rendering transparently.

## Nice-to-haves (later)
- Load PSF font from disk.
- Support multiple fonts or themes.
- Hardware cursor emulation.
