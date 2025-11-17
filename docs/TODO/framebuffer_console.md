# Framebuffer Console TODO

## Goals
- Use GOP framebuffer when booted via UEFI.
- Retain VGA text fallback for BIOS.
- Embed a simple bitmap font for early output.

## Tasks
1. [x] **Boot Info Extensions**
   - Add framebuffer fields (base, size, width, height, stride, bpp) plus a `FRAMEBUFFER_PRESENT` flag to `kernel/include/kernel/bootinfo.h`.
   - Update the loader’s boot-info header to match.
2. [x] **Loader Updates**
   - Locate GOP via `bs->LocateProtocol`.
   - Record framebuffer metadata and set the framebuffer flag before `ExitBootServices`.
3. **Kernel Detection**
   - Read the new boot-info flag.
   - If framebuffer present, initialize the framebuffer console; otherwise keep using VGA text.
4. **Bitmap Font**
   - Embed an 8×16 public-domain font as a static array (`font[256][16]`).
   - Provide a helper to fetch glyph bitmaps by ASCII code.
5. **Blitting Routine**
   - Implement `fb_draw_glyph(char c, uint32_t x, uint32_t y)` that writes pixels into the framebuffer using the GOP metadata.
   - Support foreground/background colors.
6. **Console API**
   - Implement framebuffer versions of `kprint`, `kprint_at`, newline handling, scrolling, and cursor tracking.
   - Mirror to serial for now to aid debugging.
7. **Integration**
   - Switch `kprint`/`clear_screen` to dispatch to VGA or framebuffer backend based on the boot-info flag.
   - Keep BIOS path untouched.

## Nice-to-haves (later)
- Load PSF font from disk.
- Support multiple fonts or themes.
- Hardware cursor emulation.