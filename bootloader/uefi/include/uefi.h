#ifndef CASSEOS_UEFI_H
#define CASSEOS_UEFI_H

#ifdef __GNUC__
#define UINT8  unsigned char
#define UINT16 unsigned short
#define UINT32 unsigned int
#define UINT64 unsigned long long
#define INT16  short
#define INT32  int
#define INT64  long long
#else
#error "Define fixed-width integer aliases for this compiler"
#endif

typedef void VOID;

typedef UINT8 BOOLEAN;
typedef UINT16 CHAR16;
typedef UINT64 UINTN;
typedef UINT64 EFI_STATUS;
typedef void *EFI_HANDLE;
typedef UINT64 EFI_PHYSICAL_ADDRESS;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifndef EFIAPI
#define EFIAPI __attribute__((ms_abi))
#endif

#define EFI_SUCCESS 0
#define EFI_LOAD_ERROR ((EFI_STATUS)0x8000000000000001ULL)
#define EFI_INVALID_PARAMETER ((EFI_STATUS)0x8000000000000002ULL)
#define EFI_BUFFER_TOO_SMALL ((EFI_STATUS)0x8000000000000005ULL)

#define EFI_ERROR(Status) (((Status) >> 63) != 0)

typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8 Data4[8];
} EFI_GUID;

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

typedef struct EFI_BOOT_SERVICES EFI_BOOT_SERVICES;

typedef struct {
    UINT16 Year;
    UINT8 Month;
    UINT8 Day;
    UINT8 Hour;
    UINT8 Minute;
    UINT8 Second;
    UINT8 Pad1;
    UINT32 Nanosecond;
    INT16 TimeZone;
    UINT8 Daylight;
    UINT8 Pad2;
} EFI_TIME;

typedef enum {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress,
    MaxAllocateType
} EFI_ALLOCATE_TYPE;

typedef enum {
    AllHandles,
    ByRegisterNotify,
    ByProtocol
} EFI_LOCATE_SEARCH_TYPE;

typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiPersistentMemory,
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;

typedef struct {
    UINT32 Type;
    EFI_PHYSICAL_ADDRESS PhysicalStart;
    EFI_PHYSICAL_ADDRESS VirtualStart;
    UINT64 NumberOfPages;
    UINT64 Attribute;
} EFI_MEMORY_DESCRIPTOR;

typedef EFI_STATUS(EFIAPI *EFI_ALLOCATE_PAGES)(EFI_ALLOCATE_TYPE, EFI_MEMORY_TYPE, UINTN, EFI_PHYSICAL_ADDRESS *);
typedef EFI_STATUS(EFIAPI *EFI_FREE_PAGES)(EFI_PHYSICAL_ADDRESS, UINTN);
typedef EFI_STATUS(EFIAPI *EFI_GET_MEMORY_MAP)(UINTN *, EFI_MEMORY_DESCRIPTOR *, UINTN *, UINTN *, UINT32 *);
typedef EFI_STATUS(EFIAPI *EFI_ALLOCATE_POOL)(EFI_MEMORY_TYPE, UINTN, void **);
typedef EFI_STATUS(EFIAPI *EFI_FREE_POOL)(void *);
typedef EFI_STATUS(EFIAPI *EFI_EXIT_BOOT_SERVICES)(EFI_HANDLE, UINTN);
typedef EFI_STATUS(EFIAPI *EFI_LOCATE_PROTOCOL)(const EFI_GUID *, void *, void **);
typedef VOID(EFIAPI *EFI_SET_MEM)(void *, UINTN, UINT8);
typedef VOID(EFIAPI *EFI_COPY_MEM)(void *, const void *, UINTN);
typedef EFI_STATUS(EFIAPI *EFI_HANDLE_PROTOCOL)(EFI_HANDLE, const EFI_GUID *, void **);
typedef EFI_STATUS(EFIAPI *EFI_LOCATE_HANDLE_BUFFER)(EFI_LOCATE_SEARCH_TYPE, const EFI_GUID *, void *, UINTN *, EFI_HANDLE **);

struct EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER Hdr;
    void *RaiseTPL;
    void *RestoreTPL;
    EFI_ALLOCATE_PAGES AllocatePages;
    EFI_FREE_PAGES FreePages;
    EFI_GET_MEMORY_MAP GetMemoryMap;
    EFI_ALLOCATE_POOL AllocatePool;
    EFI_FREE_POOL FreePool;
    void *CreateEvent;
    void *SetTimer;
    void *WaitForEvent;
    void *SignalEvent;
    void *CloseEvent;
    void *CheckEvent;
    void *InstallProtocolInterface;
    void *ReinstallProtocolInterface;
    void *UninstallProtocolInterface;
    EFI_HANDLE_PROTOCOL HandleProtocol;
    void *Reserved;
    void *RegisterProtocolNotify;
    void *LocateHandle;
    void *LocateDevicePath;
    void *InstallConfigurationTable;
    void *ImageLoad;
    void *ImageStart;
    void *Exit;
    void *ImageUnload;
    EFI_EXIT_BOOT_SERVICES ExitBootServices;
    void *GetNextMonotonicCount;
    void *Stall;
    void *SetWatchdogTimer;
    void *ConnectController;
    void *DisconnectController;
    void *OpenProtocol;
    void *CloseProtocol;
    void *OpenProtocolInformation;
    void *ProtocolsPerHandle;
    void *RegisterProtocolNotify2;
    EFI_LOCATE_HANDLE_BUFFER LocateHandleBuffer;
    EFI_LOCATE_PROTOCOL LocateProtocol;
    void *InstallMultipleProtocolInterfaces;
    void *UninstallMultipleProtocolInterfaces;
    void *CalculateCrc32;
    EFI_COPY_MEM CopyMem;
    EFI_SET_MEM SetMem;
    void *CreateEventEx;
};

typedef struct {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    UINT32 FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    void *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    void *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    UINTN NumberOfTableEntries;
    void *ConfigurationTable;
} EFI_SYSTEM_TABLE;

typedef struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
typedef struct EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;
typedef struct EFI_LOADED_IMAGE_PROTOCOL EFI_LOADED_IMAGE_PROTOCOL;
typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef EFI_STATUS(EFIAPI *EFI_FILE_OPEN)(EFI_FILE_PROTOCOL *, EFI_FILE_PROTOCOL **, const CHAR16 *, UINT64, UINT64);
typedef EFI_STATUS(EFIAPI *EFI_FILE_CLOSE)(EFI_FILE_PROTOCOL *);
typedef EFI_STATUS(EFIAPI *EFI_FILE_READ)(EFI_FILE_PROTOCOL *, UINTN *, void *);
typedef EFI_STATUS(EFIAPI *EFI_FILE_WRITE)(EFI_FILE_PROTOCOL *, UINTN *, const void *);
typedef EFI_STATUS(EFIAPI *EFI_FILE_GET_POSITION)(EFI_FILE_PROTOCOL *, UINT64 *);
typedef EFI_STATUS(EFIAPI *EFI_FILE_SET_POSITION)(EFI_FILE_PROTOCOL *, UINT64);
typedef EFI_STATUS(EFIAPI *EFI_FILE_GET_INFO)(EFI_FILE_PROTOCOL *, const EFI_GUID *, UINTN *, void *);
typedef EFI_STATUS(EFIAPI *EFI_FILE_FLUSH)(EFI_FILE_PROTOCOL *);

struct EFI_FILE_PROTOCOL {
    UINT64 Revision;
    EFI_FILE_OPEN Open;
    EFI_FILE_CLOSE Close;
    EFI_FILE_CLOSE Delete;
    EFI_FILE_READ Read;
    EFI_FILE_WRITE Write;
    EFI_FILE_GET_POSITION GetPosition;
    EFI_FILE_SET_POSITION SetPosition;
    EFI_FILE_GET_INFO GetInfo;
    void *SetInfo;
    EFI_FILE_FLUSH Flush;
    void *OpenEx;
    void *ReadEx;
    void *WriteEx;
    void *FlushEx;
};

typedef EFI_STATUS(EFIAPI *EFI_SIMPLE_FS_OPEN_VOLUME)(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *, EFI_FILE_PROTOCOL **);

struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    UINT64 Revision;
    EFI_SIMPLE_FS_OPEN_VOLUME OpenVolume;
};

struct EFI_LOADED_IMAGE_PROTOCOL {
    UINT32 Revision;
    EFI_HANDLE ParentHandle;
    EFI_SYSTEM_TABLE *SystemTable;
    EFI_HANDLE DeviceHandle;
    void *FilePath;
    void *Reserved;
    UINT32 LoadOptionsSize;
    void *LoadOptions;
    void *ImageBase;
    UINT64 ImageSize;
    EFI_MEMORY_TYPE ImageCodeType;
    EFI_MEMORY_TYPE ImageDataType;
    EFI_STATUS(EFIAPI *Unload)(EFI_HANDLE ImageHandle);
};

typedef struct {
    UINT32 Version;
    UINT32 HorizontalResolution;
    UINT32 VerticalResolution;
    UINT32 PixelFormat;
    UINT32 PixelsPerScanLine;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    UINT32 MaxMode;
    UINT32 Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINTN SizeOfInfo;
    EFI_PHYSICAL_ADDRESS FrameBufferBase;
    UINTN FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL {
    EFI_STATUS (EFIAPI *QueryMode)(struct EFI_GRAPHICS_OUTPUT_PROTOCOL *, UINT32, UINTN *, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **);
    EFI_STATUS (EFIAPI *SetMode)(struct EFI_GRAPHICS_OUTPUT_PROTOCOL *, UINT32);
    void *Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef struct {
    UINT64 Size;
    UINT64 FileSize;
    UINT64 PhysicalSize;
    EFI_TIME CreateTime;
    EFI_TIME LastAccessTime;
    EFI_TIME ModificationTime;
    UINT64 Attribute;
    CHAR16 FileName[1];
} EFI_FILE_INFO;

#define EFI_FILE_MODE_READ 0x0000000000000001ULL

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table);

#endif /* CASSEOS_UEFI_H */
