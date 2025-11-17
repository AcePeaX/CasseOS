#ifndef CASSEOS_UEFI_GOP_H
#define CASSEOS_UEFI_GOP_H

#include "uefi.h"

typedef struct {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    UINT32 width;
    UINT32 height;
    UINT32 pixels_per_scanline;
    EFI_PHYSICAL_ADDRESS framebuffer_base;
    UINTN framebuffer_size;
} gop_info_t;

EFI_STATUS locate_gop(EFI_SYSTEM_TABLE *system_table, gop_info_t *out_info);

#endif
