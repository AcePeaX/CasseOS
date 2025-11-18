# UEFI Framebuffer Console Design

## Table of Contents
1. [Overview](#overview)
2. [Framebuffer Basics](#framebuffer-basics)
3. [Obtaining GOP Information](#obtaining-gop-information)
4. [Text Rendering Pipeline](#text-rendering-pipeline)
    1. [Font Selection](#font-selection)
    2. [Glyph Blitting](#glyph-blitting)
    3. [Console State Management](#console-state-management)
5. [Serial Output as Fallback](#serial-output-as-fallback)
6. [Implementation Plan](#implementation-plan)

## Overview
BIOS boot enabled CasseOS to write text directly into VGA text memory at `0xB8000`. Under UEFI/OVMF, the firmware switches to GOP (Graphics Output Protocol) and no longer maps VGA text mode, so writes to `0xB8000` have no visible effect. To support output on UEFI systems, CasseOS must either:
- Render text into the GOP framebuffer (preferred), or
- Use another device (e.g., serial port) for console output.
This document describes the architecture for a GOP-based framebuffer console and notes how to keep serial output as a fallback for early debugging.

## Framebuffer Basics
- GOP exposes a linear framebuffer: a block of memory representing the screen’s pixels.
- Key fields from `EFI_GRAPHICS_OUTPUT_PROTOCOL`:
  - `FrameBufferBase`: physical address of the framebuffer.
  - `FrameBufferSize`: total size in bytes.
  - `Mode->Info->HorizontalResolution` and `VerticalResolution`.
  - `Mode->Info->PixelsPerScanLine` (stride).
  - Pixel format (typically 32-bit BGRA).
- To draw, the kernel writes color values directly into this memory.

## Obtaining GOP Information
1. While still in boot services, locate GOP:
   ```c
   EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
   EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
   EFI_STATUS status = bs->LocateProtocol(&gopGuid, NULL, (void **)&gop);
   ```
   The official structure layout (with the embedded `EFI_PIXEL_BITMASK`) and additional discussion about mode handling live on the [OSDev GOP page](https://wiki.osdev.org/GOP); double-checking against that reference avoids field-offset mistakes.
2. Record framebuffer base, size, resolution, and stride in the boot info structure before `ExitBootServices`.
3. Pass these values to the kernel via the existing boot-info struct so the kernel can access the framebuffer after the loader exits.

## Text Rendering Pipeline
Rendering text on a framebuffer requires a software text console. Core components:

### Font Selection
- Use a bitmap font (e.g., 8×16 or 8×8) encoded as a byte array where each bit represents a pixel.
- Store glyphs in a table indexed by ASCII code.
- Choose a public-domain VGA/BIOS font for simplicity.

### Glyph Blitting
- For each character to display:
  1. Look up its glyph bitmap.
  2. For each bit set in the glyph, write the foreground color to the corresponding pixel in the framebuffer; write background color otherwise.
  3. Pixel address calculation: `pixel_ptr = FrameBufferBase + (y * PixelsPerScanLine + x) * BytesPerPixel`.
- Support configurable foreground/background colors.

### Console State Management
- Maintain a cursor (row/column) measured in character cells.
- Implement `kprint`, `kprint_at`, newline handling, and scrolling by copying framebuffer rows upward when the cursor reaches the bottom.
- Optionally implement double-buffering or partial redraws for efficiency.

## Serial Output as Fallback
- During bring-up (before the framebuffer console is complete) or for persistent logging, use the first serial port (`0x3F8`) for output.
- Serial works under both BIOS and UEFI without graphical setup.
- Implement simple serial routines (`serial_write_char`, `serial_write_string`) and optionally tie them into the logging infrastructure so kernel messages go to both framebuffer and serial.

## Implementation Plan
1. Loader changes:
   - Locate GOP.
   - Capture framebuffer base, size, resolution, stride, and pixel format in the boot-info flags.
2. Boot info struct:
   - Extend to include framebuffer metadata.
   - Add flags to describe which console backends are initialized (e.g., `FRAMEBUFFER_PRESENT`).
3. Kernel console:
   - Add a framebuffer console module that takes the GOP metadata and exposes `fb_put_char`, `fb_clear`, etc.
   - Update `kprint` to call the framebuffer console when the framebuffer flag is set; otherwise fallback to serial.
4. Serial logging:
   - Implement a lightweight serial writer for early debugging.
   - Optionally mirror all `kprint` output to serial until the framebuffer console is proven stable.
5. Testing:
   - Boot via UEFI, confirm the loader prints to firmware console, then verify kernel text appears via the framebuffer console.
   - Boot via BIOS to ensure VGA path still works.
