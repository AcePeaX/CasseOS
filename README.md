# CasseOS
CasseOS is my custom OS, as an experiment.

## Commands

```bash
# Compile the bootloader
nasm -f bin boot.asm -o ./bin/boot.bin
# Create virtual disk image
dd if=/dev/zero of=boot.img bs=512 count=2880
# Write bootloader into the image
dd if=bin/boot.bin of=boot.img conv=notrunc
# Convert img to vdi
VBoxManage convertdd boot.img boot.vdi --format VDI
```