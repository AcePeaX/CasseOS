#include "uefi.h"

static const CHAR16 HELLO_MESSAGE[] = L"Hello bootloader\r\n";

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table) {
    (void)image_handle;

    if (system_table == (void *)0) {
        return EFI_SUCCESS;
    }

    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *console = system_table->ConOut;
    if (console == (void *)0 || console->OutputString == (void *)0) {
        return EFI_SUCCESS;
    }

    console->OutputString(console, (CHAR16 *)HELLO_MESSAGE);
    return EFI_SUCCESS;
}
