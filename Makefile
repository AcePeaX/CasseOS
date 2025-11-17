# $@ = target file
# $< = first dependency
# $^ = all dependencies

GCC=x86_64-elf-gcc # Or /usr/local/cross/bin/x86_64-elf-gcc
#GCC=i386-elf-gcc or /usr/local/cross/bin/i386-elf-gcc
#GCC="i386-elf-gcc" or /usr/local/cross/bin/i686-elf-gcc

LD=x86_64-elf-ld # Or /usr/local/cross/bin/x86_64-elf-ld
#LD=i386-elf-ld or /usr/local/cross/bin/i386-elf-ld
#LD=i686-elf-ld or /usr/local/cross/bin/i686-elf-ld

UEFI_LD ?= ld

GDB=x86_64-elf-gdb # Or /usr/local/cross/bin/x86_64-elf-gdb
#GDB=i386-elf-gdb or /usr/local/cross/bin/i386-elf-gdb
#GDB="i686-elf-gdb" or /usr/local/cross/bin/i686-elf-gdb

OBJCOPY=x86_64-elf-objcopy

#QEMU=qemu-system-i386
QEMU=qemu-system-x86_64

KERNEL_START_MEM = 0x80000

BUILD_DIR := .build
BIN_DIR := .bin

DISK_IMAGE := $(BIN_DIR)/casseos.img

BIOS_BOOTLOADER_DIR := bootloader/bios
BIOS_BOOTLOADER_SRC := $(BIOS_BOOTLOADER_DIR)/bootloader.asm
BIOS_BOOTLOADER_FILES := $(wildcard $(BIOS_BOOTLOADER_DIR)/*.asm)

UEFI_DIR := bootloader/uefi
UEFI_INCLUDE := -I$(UEFI_DIR)/include
UEFI_OBJ := $(BUILD_DIR)/bootloader/uefi/main.o
UEFI_EFI := $(BIN_DIR)/BOOTX64.EFI
UEFI_HEADERS := $(UEFI_DIR)/include/uefi.h
UEFI_CFLAGS := ${CFLAGS} -fshort-wchar -mno-red-zone -fno-stack-protector -fno-ident -fpic $(UEFI_INCLUDE)
UEFI_LDFLAGS := -nostdlib -shared -Bsymbolic -m i386pep --subsystem 10 -e efi_main

C_SOURCES = $(shell find kernel drivers cpu libc -name '*.c')
#C_SOURCES = $(shell find kernel cpu libc -name '*.c')
HEADERS = $(shell find kernel drivers cpu libc -name '*.h')
#HEADERS = $(shell find kernel cpu drivers -name '*.h')
# Nice syntax for file extension replacement
OBJ := $(patsubst %.c, $(BUILD_DIR)/%.o, $(C_SOURCES) $(BUILD_DIR)/cpu/interrupt.o)
#OBJ := $(patsubst %.c, $(BUILD_DIR)/%.o, $(C_SOURCES))


#$(info OBJ files: $(OBJ))
# -g: Use debugging symbols in gcc
CFLAGS = -g -ffreestanding -Wall -Wextra -fno-exceptions -m64 -I. -O2
LDFLAGS = -T linker.ld
QEMUFLAGS = -device piix3-usb-uhci \
		#-machine pc \
		#-device usb-kbd \
		# -device usb-mouse \
		#-trace usb_uhci
EXTRA_QEMU_FLAGS ?=

all: os-image $(UEFI_EFI)

$(BIN_DIR)/kernel.bin: $(BUILD_DIR)/kernel/kernel_entry.o ${OBJ}
	$(LD) $(LDFLAGS) -o $@ -Ttext $(KERNEL_START_MEM) $^ --oformat binary

$(BIN_DIR)/bootloader.bin: $(BIOS_BOOTLOADER_FILES)
	@./scripts/create_file_path.sh $@
	@nasm $(BIOS_BOOTLOADER_SRC) -f bin -i$(BIOS_BOOTLOADER_DIR)/ -o $@

$(BUILD_DIR)/kernel.elf: $(BUILD_DIR)/kernel/kernel_entry.o ${OBJ}
	@$(LD) $(LDFLAGS) -o $@ -Ttext $(KERNEL_START_MEM) $^

kernel.bin: $(BIN_DIR)/kernel.bin
bootloader.bin: $(BIN_DIR)/bootloader.bin
kernel: kernel.bin
bootloader: bootloader.bin


$(BIN_DIR)/os-image.bin: $(BIN_DIR)/bootloader.bin $(BIN_DIR)/kernel.bin
	@python3 ./scripts/check-size-matching.py $(BIN_DIR)/kernel.bin
	@cat $^ > $(BIN_DIR)/os-image.bin
	@echo "Successfully compiled the OS"
os-image.bin: $(BIN_DIR)/os-image.bin
os-image: os-image.bin

$(UEFI_OBJ): $(UEFI_DIR)/main.c $(UEFI_HEADERS)
	@./scripts/create_file_path.sh $@
	$(GCC) $(UEFI_CFLAGS) -c $< -o $@

$(UEFI_EFI): $(UEFI_OBJ)
	@./scripts/create_file_path.sh $@
	$(UEFI_LD) $(UEFI_LDFLAGS) $(UEFI_OBJ) -o $@

$(DISK_IMAGE): $(BIN_DIR)/os-image.bin $(UEFI_EFI) scripts/build_disk_image.sh
	@./scripts/build_disk_image.sh

disk-image: $(DISK_IMAGE)

qemu-bios: $(BIN_DIR)/os-image.bin
	@$(QEMU) $(QEMUFLAGS) -fda $(BIN_DIR)/os-image.bin -monitor stdio -display sdl $(EXTRA_QEMU_FLAGS)

qemu: qemu-bios

run: qemu -device usb-tablet

debug: $(BUILD_DIR)/kernel.elf $(BIN_DIR)/os-image.bin
	$(QEMU) $(QEMUFLAGS) -fda $(BIN_DIR)/os-image.bin -monitor stdio -display sdl -s -S &
	${GDB} -ex "target remote localhost:1234" -ex "symbol-file .build/kernel.elf"

virtualbox: $(BIN_DIR)/os-image.bin
	@./scripts/launch.sh --virtualbox

OVMF_CODE ?= /usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS_TEMPLATE ?= /usr/share/OVMF/OVMF_VARS_4M.fd
OVMF_VARS ?= $(BIN_DIR)/OVMF_VARS.fd

qemu-uefi: $(DISK_IMAGE)
	@if [ ! -f "$(OVMF_CODE)" ]; then echo "Missing OVMF_CODE at $(OVMF_CODE). Override the variable to point to your OVMF_CODE.fd."; exit 1; fi
	@if [ ! -f "$(OVMF_VARS_TEMPLATE)" ] && [ ! -f "$(OVMF_VARS)" ]; then echo "Missing OVMF_VARS template at $(OVMF_VARS_TEMPLATE). Override OVMF_VARS_TEMPLATE or place a vars file at $(OVMF_VARS)."; exit 1; fi
	@if [ ! -f "$(OVMF_VARS)" ] && [ -f "$(OVMF_VARS_TEMPLATE)" ]; then cp "$(OVMF_VARS_TEMPLATE)" "$(OVMF_VARS)"; fi
	@$(QEMU) -cpu qemu64 \
		-drive if=pflash,format=raw,unit=0,file=$(OVMF_CODE),readonly=on \
		-drive if=pflash,format=raw,unit=1,file=$(OVMF_VARS) \
		-drive format=raw,file=$(DISK_IMAGE) \
		-net none $(EXTRA_QEMU_FLAGS)

num_sectors: $(BIN_DIR)/kernel.bin
	@KERNEL_BIN_PATH=$(BIN_DIR)/kernel.bin ./scripts/num_sectors.sh

info:
	@echo "C_SOURCES = $(C_SOURCES)"
	@echo "OBJ = $(OBJ)"

bootloader-elf: $(BIOS_BOOTLOADER_FILES)
	nasm -f elf32 -g $(BIOS_BOOTLOADER_SRC) -i$(BIOS_BOOTLOADER_DIR)/ -o $(BUILD_DIR)/bootloader.o  -D ELF_FORMAT
	ld -m elf_i386 -Ttext 0x7C00 -o $(BIN_DIR)/bootloader-elf $(BUILD_DIR)/bootloader.o
	objdump -d $(BIN_DIR)/bootloader-elf

# Generic rules for wildcards
# To make an object, always compile from its .c
$(BUILD_DIR)/%.o: %.c ${HEADERS}
	@./scripts/create_file_path.sh $@
	$(GCC) ${CFLAGS} -ffreestanding -c $< -o $@


$(BUILD_DIR)/%.o: %.asm
	@./scripts/create_file_path.sh $@
	nasm $< -f elf64 -i$(dir $<) -D ELF_FORMAT -o $@

$(BIN_DIR)/%.bin: %.asm
	nasm $< -f bin -o $@



gdb:
	gdb

clean:
	@rm -r $(BUILD_DIR)/* $(BIN_DIR)/*
