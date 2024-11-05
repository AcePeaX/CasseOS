# $@ = target file
# $< = first dependency
# $^ = all dependencies


GCC=i386-elf-gcc # Or /usr/local/cross/bin/i386-elf-gcc
# GCC="i386-elf-gcc" or /usr/local/cross/bin/i686-elf-gcc

LD=i386-elf-ld # Or /usr/local/cross/bin/i386-elf-ld
#LD="i686-elf-ld" or /usr/local/cross/bin/i686-elf-ld

GDB=i386-elf-gdb # Or /usr/local/cross/bin/i386-elf-gdb
#GDB="i686-elf-gdb" or /usr/local/cross/bin/i686-elf-gdb


BUILD_DIR := .build
BIN_DIR := .bin

C_SOURCES = $(wildcard kernel/*.c drivers/*.c)
HEADERS = $(wildcard kernel/*.h drivers/*.h)
# Nice syntax for file extension replacement
OBJ := $(patsubst %.c, $(BUILD_DIR)/%.o, $(C_SOURCES))

#$(info OBJ files: $(OBJ))
# -g: Use debugging symbols in gcc
CFLAGS = -g

all: os-image

$(BIN_DIR)/kernel.bin: $(BUILD_DIR)/kernel/kernel_entry.o ${OBJ}
	@$(LD) -o $@ -Ttext 0x1000 $^ --oformat binary

$(BIN_DIR)/bootloader.bin: bootloader/*
	@./scripts/create_file_path.sh $@
	@nasm bootloader/bootloader.asm -f bin -i$(dir $<) -o $@

$(BUILD_DIR)/kernel.elf: $(BUILD_DIR)/kernel/kernel_entry.o ${OBJ}
	@$(LD) -o $@ -Ttext 0x1000 $^

kernel.bin: $(BIN_DIR)/kernel.bin
bootloader.bin: $(BIN_DIR)/bootloader.bin
kernel: kernel.bin
bootloader: bootloader.bin


$(BIN_DIR)/os-image.bin: $(BIN_DIR)/bootloader.bin $(BIN_DIR)/kernel.bin
	cat $^ > $(BIN_DIR)/os-image.bin
os-image.bin: $(BIN_DIR)/os-image.bin
os-image: os-image.bin

qemu: $(BIN_DIR)/os-image.bin
	@./scripts/launch.sh

run: qemu

debug: $(BIN_DIR)/os-image.bin $(BUILD_DIR)/kernel.elf
	@./scripts/launch.sh --debug &
	${GDB} -ex "target remote localhost:1234" -ex "symbol-file .build/kernel.elf"

virtualbox: $(BIN_DIR)/os-image.bin
	@./scripts/launch.sh --virtualbox


# Generic rules for wildcards
# To make an object, always compile from its .c
$(BUILD_DIR)/%.o: %.c ${HEADERS}
	@./scripts/create_file_path.sh $@
	@$(GCC) ${CFLAGS} -ffreestanding -c $< -o $@


$(BUILD_DIR)/%.o: %.asm
	@./scripts/create_file_path.sh $@
	@nasm $< -f elf -i$(dir $<) -D ELF_FORMAT -o $@

$(BIN_DIR)/%.bin: %.asm
	@nasm $< -f bin -o $@



gdb:
	gdb

clean:
	@rm -r $(BUILD_DIR)/* $(BIN_DIR)/*
