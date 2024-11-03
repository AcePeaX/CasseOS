# CasseOS
CasseOS is my custom OS, as an experiment.

## Commands

To start
```bash
# Compile the bootloader
nasm -f bin boot.asm -o ./bin/boot.bin
# Create virtual disk image
dd if=/dev/zero of=./bin/boot.img bs=512 count=2880
# Write bootloader into the image
dd if=bin/boot.bin of=./bin/boot.img conv=notrunc
# Convert img to vdi
VBoxManage convertdd ./bin/boot.img ./bin/boot.vdi --format VDI
```

To update the VM in virtual box:

```bash
$VM_NAME=CasseOS    # Put yur VM name
VBoxManage convertfromraw ./bin/$OUTPUT_BIN ./bin/$DISK_IMAGE --format VDI
./scripts/updateVM.sh
```