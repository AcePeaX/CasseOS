#include <stddef.h>
#include "gop.h"

EFI_STATUS locate_gop(EFI_SYSTEM_TABLE *system_table, gop_info_t *out_info) {
    if (!out_info) return EFI_INVALID_PARAMETER;

    EFI_GUID gopGuid =  {0x9042a9de,0x23dc,0x4a38,{0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a}};
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_STATUS status = system_table->BootServices->LocateProtocol(&gopGuid, NULL, (void **)&gop);
    if (EFI_ERROR(status) || gop == NULL) {
        return status;
    }

    out_info->gop = gop;
    out_info->width = gop->Mode->Info->HorizontalResolution;
    out_info->height = gop->Mode->Info->VerticalResolution;
    out_info->pixels_per_scanline = gop->Mode->Info->PixelsPerScanLine;
    out_info->framebuffer_base = gop->Mode->FrameBufferBase;
    out_info->framebuffer_size = gop->Mode->FrameBufferSize;

    return EFI_SUCCESS;
}
