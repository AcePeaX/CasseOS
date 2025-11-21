#include <stddef.h>
#include "gop.h"

EFI_STATUS locate_gop(EFI_SYSTEM_TABLE *system_table, gop_info_t *out_info) {
    if (!out_info) return EFI_INVALID_PARAMETER;

    EFI_GUID gopGuid =  {0x9042a9de,0x23dc,0x4a38,{0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a}};
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_STATUS status = system_table->BootServices->LocateProtocol(&gopGuid, NULL, (void **)&gop);
    if (EFI_ERROR(status) || gop == NULL) {
        EFI_HANDLE *handles = NULL;
        UINTN handle_count = 0;
        status = system_table->BootServices->LocateHandleBuffer(ByProtocol, &gopGuid, NULL, &handle_count, &handles);
        if (EFI_ERROR(status)) {
            return status;
        }
        for (UINTN i = 0; i < handle_count; ++i) {
            EFI_STATUS proto_status = system_table->BootServices->HandleProtocol(handles[i], &gopGuid, (void **)&gop);
            if (!EFI_ERROR(proto_status) && gop != NULL) {
                status = EFI_SUCCESS;
                break;
            }
        }
        system_table->BootServices->FreePool(handles);
        if (EFI_ERROR(status) || gop == NULL) {
            return status;
        }
    }

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *mode_info = NULL;
    UINTN mode_info_size = 0;
    status = gop->QueryMode(gop, gop->Mode->Mode, &mode_info_size, &mode_info);
    if (EFI_ERROR(status)) {
        return status;
    }

    out_info->gop = gop;
    out_info->width = mode_info->HorizontalResolution;
    out_info->height = mode_info->VerticalResolution;
    out_info->pixels_per_scanline = mode_info->PixelsPerScanLine;
    out_info->framebuffer_base = gop->Mode->FrameBufferBase;
    out_info->framebuffer_size = gop->Mode->FrameBufferSize;
    system_table->BootServices->FreePool(mode_info);

    return EFI_SUCCESS;
}
