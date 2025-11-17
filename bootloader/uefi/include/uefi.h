#ifndef CASSEOS_UEFI_H
#define CASSEOS_UEFI_H

#ifdef __GNUC__
#define UINT8  unsigned char
#define UINT16 unsigned short
#define UINT32 unsigned int
#define UINT64 unsigned long long
#else
#error "Define fixed-width integer aliases for this compiler"
#endif

typedef UINT8 BOOLEAN;
typedef UINT16 CHAR16;
typedef UINT64 UINTN;
typedef UINT64 EFI_STATUS;
typedef void *EFI_HANDLE;

#ifndef EFIAPI
#define EFIAPI __attribute__((ms_abi))
#endif

#define EFI_SUCCESS 0

typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef EFI_STATUS(EFIAPI *EFI_TEXT_RESET)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *, BOOLEAN);
typedef EFI_STATUS(EFIAPI *EFI_TEXT_STRING)(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *, const CHAR16 *);

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_TEXT_RESET Reset;
    EFI_TEXT_STRING OutputString;
    void *TestString;
    void *QueryMode;
    void *SetMode;
    void *SetAttribute;
    void *ClearScreen;
    void *SetCursorPosition;
    void *EnableCursor;
    void *Mode;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    UINT32 FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    void *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
} EFI_SYSTEM_TABLE;

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table);

#endif /* CASSEOS_UEFI_H */
