#include "uefi.h"

static const CHAR16 HELLO_MESSAGE[] = {
    'H','e','l','l','o',' ','b','o','o','t','l','o','a','d','e','r','\r','\n',0
};

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
