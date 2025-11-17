#define NULL ((void *)0)
#include "kernel_bootinfo.h"
#include "uefi.h"

#define KERNEL_RELATIVE_PATH L"\\CASSEKRN.BIN"
#define KERNEL_LOAD_ADDRESS 0x0000000000080000ULL
#define KERNEL_STACK_PAGES 16
#define PAGE_SIZE 4096ULL

static const EFI_GUID gEfiSimpleFileSystemProtocolGuid = {
    0x964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

static const EFI_GUID gEfiFileInfoGuid = {
    0x09576e92, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

static const EFI_GUID gEfiLoadedImageProtocolGuid = {
    0x5b1b31a1, 0x9562, 0x11d2, {0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

static void print(EFI_SYSTEM_TABLE *system_table, const CHAR16 *message) {
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *console = system_table->ConOut;
    if (console && console->OutputString) {
        console->OutputString(console, message);
    }
}

static EFI_STATUS get_file_size(EFI_BOOT_SERVICES *bs, EFI_FILE_PROTOCOL *file, UINTN *size) {
    UINTN info_size = 0;
    EFI_FILE_INFO *info = NULL;
    EFI_STATUS status = file->GetInfo(file, &gEfiFileInfoGuid, &info_size, NULL);
    if (status != EFI_BUFFER_TOO_SMALL) {
        return status;
    }

    status = bs->AllocatePool(EfiLoaderData, info_size, (void **)&info);
    if (EFI_ERROR(status)) {
        return status;
    }

    status = file->GetInfo(file, &gEfiFileInfoGuid, &info_size, info);
    if (!EFI_ERROR(status)) {
        *size = (UINTN)info->FileSize;
    }
    bs->FreePool(info);
    return status;
}

static EFI_STATUS load_kernel(EFI_SYSTEM_TABLE *system_table,
                              EFI_FILE_PROTOCOL *root,
                              EFI_PHYSICAL_ADDRESS *kernel_address,
                              UINTN *kernel_pages) {
    EFI_BOOT_SERVICES *bs = system_table->BootServices;
    EFI_FILE_PROTOCOL *kernel_file = NULL;
    EFI_STATUS status = root->Open(root, &kernel_file, KERNEL_RELATIVE_PATH, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        return status;
    }

    UINTN file_size = 0;
    status = get_file_size(bs, kernel_file, &file_size);
    if (EFI_ERROR(status)) {
        kernel_file->Close(kernel_file);
        return status;
    }

    UINTN pages = (file_size + PAGE_SIZE - 1) / PAGE_SIZE;
    EFI_PHYSICAL_ADDRESS destination = KERNEL_LOAD_ADDRESS;
    status = bs->AllocatePages(AllocateAddress, EfiLoaderData, pages, &destination);
    if (EFI_ERROR(status)) {
        kernel_file->Close(kernel_file);
        return status;
    }

    UINTN remaining = file_size;
    UINT8 *write_ptr = (UINT8 *)(UINTN)destination;
    while (remaining > 0) {
        UINTN chunk = remaining;
        status = kernel_file->Read(kernel_file, &chunk, write_ptr);
        if (EFI_ERROR(status)) {
            kernel_file->Close(kernel_file);
            return status;
        }
        if (chunk == 0) {
            break;
        }
        remaining -= chunk;
        write_ptr += chunk;
    }
    kernel_file->Close(kernel_file);

    if (remaining != 0) {
        return EFI_LOAD_ERROR;
    }

    *kernel_address = destination;
    *kernel_pages = pages;
    return EFI_SUCCESS;
}

static EFI_STATUS exit_boot_services(EFI_BOOT_SERVICES *bs,
                                     EFI_HANDLE image_handle) {
    EFI_STATUS status;
    UINTN map_size = 0;
    UINTN map_key = 0;
    UINTN descriptor_size = 0;
    UINT32 descriptor_version = 0;
    EFI_MEMORY_DESCRIPTOR *memory_map = NULL;

    while (1) {
        status = bs->GetMemoryMap(&map_size, memory_map, &map_key, &descriptor_size, &descriptor_version);
        if (status == EFI_BUFFER_TOO_SMALL) {
            if (memory_map) {
                bs->FreePool(memory_map);
            }
            map_size += descriptor_size * 2;
            status = bs->AllocatePool(EfiLoaderData, map_size, (void **)&memory_map);
            if (EFI_ERROR(status)) {
                return status;
            }
            continue;
        } else if (EFI_ERROR(status)) {
            if (memory_map) {
                bs->FreePool(memory_map);
            }
            return status;
        }

        status = bs->ExitBootServices(image_handle, map_key);
        if (EFI_ERROR(status)) {
            if (status == EFI_INVALID_PARAMETER) {
                continue;
            }
            if (memory_map) {
                bs->FreePool(memory_map);
            }
            return status;
        }
        break;
    }

    return EFI_SUCCESS;
}

typedef void (*kernel_entry_t)(void);
extern void uefi_enter_kernel(kernel_entry_t entry, UINT64 stack_top) __attribute__((noreturn));

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table) {
    print(system_table, L"CasseOS UEFI loader starting...\r\n");
    EFI_BOOT_SERVICES *bs = system_table->BootServices;

    EFI_HANDLE *handles = NULL;
    UINTN handle_count = 0;
    EFI_STATUS status = EFI_LOAD_ERROR;
    EFI_PHYSICAL_ADDRESS kernel_location = 0;
    UINTN kernel_pages = 0;
    int kernel_loaded = FALSE;

    status = bs->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid,
                                    NULL, &handle_count, &handles);
    if (EFI_ERROR(status)) {
        /* Fallback: try to open the first loaded image's device */
        EFI_LOADED_IMAGE_PROTOCOL *loaded_image = NULL;
        status = bs->HandleProtocol(image_handle, &gEfiLoadedImageProtocolGuid, (void **)&loaded_image);
        if (EFI_ERROR(status) || loaded_image == NULL) {
            print(system_table, L"Failed to query loaded image protocol\r\n");
            return status;
        }

        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
        status = bs->HandleProtocol(loaded_image->DeviceHandle,
                                    &gEfiSimpleFileSystemProtocolGuid, (void **)&fs);
        if (EFI_ERROR(status)) {
            print(system_table, L"Failed to enumerate filesystem handles\r\n");
            return status;
        }

        EFI_FILE_PROTOCOL *root = NULL;
        status = fs->OpenVolume(fs, &root);
        if (EFI_ERROR(status)) {
            print(system_table, L"Failed to open filesystem volume\r\n");
            return status;
        }

        status = load_kernel(system_table, root, &kernel_location, &kernel_pages);
        if (EFI_ERROR(status)) {
            print(system_table, L"Kernel not found on boot volume\r\n");
            return status;
        }

        handles = NULL;
        handle_count = 0;
        kernel_loaded = TRUE;
        goto kernel_loaded;
    }

    for (UINTN i = 0; i < handle_count; ++i) {
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
        status = bs->HandleProtocol(handles[i], &gEfiSimpleFileSystemProtocolGuid, (void **)&fs);
        if (EFI_ERROR(status)) {
            continue;
        }

        EFI_FILE_PROTOCOL *root = NULL;
        status = fs->OpenVolume(fs, &root);
        if (EFI_ERROR(status)) {
            continue;
        }

        status = load_kernel(system_table, root, &kernel_location, &kernel_pages);
        if (!EFI_ERROR(status)) {
            kernel_loaded = TRUE;
            break;
        }
    }

    if (handles) {
        bs->FreePool(handles);
    }

    if (!kernel_loaded) {
        print(system_table, L"Failed to locate kernel on any filesystem\r\n");
        return status;
    }

kernel_loaded:
    {
    kernel_bootinfo_t *boot_info = (kernel_bootinfo_t *)(UINTN)kernel_location;
    if (boot_info->magic != KERNEL_BOOTINFO_MAGIC) {
        print(system_table, L"Invalid kernel image (bootinfo magic mismatch)\r\n");
        return EFI_LOAD_ERROR;
    }
    boot_info->flags |= KERNEL_BOOTINFO_FLAG_UEFI;

    EFI_PHYSICAL_ADDRESS stack_base = 0;
    status = bs->AllocatePages(AllocateAnyPages, EfiLoaderData, KERNEL_STACK_PAGES, &stack_base);
    if (EFI_ERROR(status)) {
        print(system_table, L"Failed to allocate kernel stack\r\n");
        return status;
    }
    UINT64 stack_top = stack_base + (KERNEL_STACK_PAGES * PAGE_SIZE);
    bs->SetMem((void *)(UINTN)stack_base, KERNEL_STACK_PAGES * PAGE_SIZE, 0);

    print(system_table, L"Exiting boot services\r\n");
    status = exit_boot_services(bs, image_handle);
    if (EFI_ERROR(status)) {
        return status;
    }

    kernel_entry_t entry = (kernel_entry_t)(UINTN)boot_info->uefi_entry;
    uefi_enter_kernel(entry, stack_top);
    __builtin_unreachable();
    }
}
