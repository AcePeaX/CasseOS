# $@ = target file
# $< = first dependency
# $^ = all dependencies

GCC=x86_64-elf-gcc # Or /usr/local/cross/bin/x86_64-elf-gcc
#GCC=i386-elf-gcc or /usr/local/cross/bin/i386-elf-gcc
#GCC="i386-elf-gcc" or /usr/local/cross/bin/i686-elf-gcc

LD=x86_64-elf-ld # Or /usr/local/cross/bin/x86_64-elf-ld
#LD=i386-elf-ld or /usr/local/cross/bin/i386-elf-ld
#LD=i686-elf-ld or /usr/local/cross/bin/i686-elf-ld

GDB=x86_64-elf-gdb # Or /usr/local/cross/bin/x86_64-elf-gdb
#GDB=i386-elf-gdb or /usr/local/cross/bin/i386-elf-gdb
#GDB="i686-elf-gdb" or /usr/local/cross/bin/i686-elf-gdb

#QEMU=qemu-system-i386
QEMU=qemu-system-x86_64

KERNEL_START_MEM = 0x80000

BUILD_DIR := .build
BIN_DIR := .bin


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
QEMUFLAGS = -usb -device usb-kbd

all: os-image

$(BIN_DIR)/kernel.bin: $(BUILD_DIR)/kernel/kernel_entry.o ${OBJ}
	$(LD) $(LDFLAGS) -o $@ -Ttext $(KERNEL_START_MEM) $^ --oformat binary

$(BIN_DIR)/bootloader.bin: bootloader/*
	@./scripts/create_file_path.sh $@
	@nasm bootloader/bootloader.asm -f bin -i$(dir $<) -o $@

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

qemu: $(BIN_DIR)/os-image.bin
	@$(QEMU) $(QEMUFLAGS) -fda $(BIN_DIR)/os-image.bin -monitor stdio -display sdl

run: qemu -device usb-tablet

debug: $(BUILD_DIR)/kernel.elf $(BIN_DIR)/os-image.bin
	$(QEMU) $(QEMUFLAGS) -fda $(BIN_DIR)/os-image.bin -monitor stdio -display sdl -s -S &
	${GDB} -ex "target remote localhost:1234" -ex "symbol-file .build/kernel.elf"

virtualbox: $(BIN_DIR)/os-image.bin
	@./scripts/launch.sh --virtualbox

num_sectors: $(BIN_DIR)/kernel.bin
	@KERNEL_BIN_PATH=$(BIN_DIR)/kernel.bin ./scripts/num_sectors.sh

info:
	@echo "C_SOURCES = $(C_SOURCES)"
	@echo "OBJ = $(OBJ)"

bootloader-elf: bootloader/*
	nasm -f elf32 -g bootloader/bootloader.asm -i$(dir $<) -o $(BUILD_DIR)/bootloader.o  -D ELF_FORMAT
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
