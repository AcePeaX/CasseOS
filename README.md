# CasseOS
CasseOS is my custom OS, as an experiment.

## Useful commands

> Note: You need first to install some cross compilers and debuggers, follow guide [here](#install-development-environment)

There is a makefile containing the commands:

- `GCC` and `LD` and `GDB` and `QEMU` variables to set to either `i386` or `i686` cross compiler/binutils/debugger
- `make clean`
- `make all`
- `make qemu-bios` boots the legacy BIOS path.
- `make run` or `make qemu`
- `make debug`
- `make virtualbox`
- `make kernel`
- `make bootloader`
- `make disk-image` builds `.bin/casseos.img` containing the BIOS loader plus a FAT32 ESP placeholder.
- `make qemu-uefi` launches QEMU with OVMF using that hybrid image; the rule auto-copies `/usr/share/OVMF/OVMF_VARS_4M.fd` into `.bin/OVMF_VARS.fd` so the mutable variable store stays inside the repo (override `OVMF_CODE`, `OVMF_VARS_TEMPLATE`, or `OVMF_VARS` if needed).

> [!NOTE]
> By default the UEFI build invokes `/usr/bin/ld -m i386pep` to emit a PE/COFF image directly. If your linker does not support that emulation, install `lld` (via the `lld` package) and set `UEFI_LD=ld.lld` when running `make`.

> [!IMPORTANT]  
> Use regularly `make num_sectors` to verify the kernel's size. If it changes, change it in `bootloader/bios/bootloader.asm`, specifically `NUM_SECTORS` at the start of the file to the correct value.

### Bootloader layout
- `bootloader/bios/` keeps the existing BIOS/legacy loader sources.
- `bootloader/uefi/` is reserved for the upcoming native UEFI loader.

The helper script `scripts/build_disk_image.sh` assembles both boot paths into `.bin/casseos.img` by placing the BIOS boot sector + kernel at the front of the disk while reserving a 64 MB FAT32 ESP that carries the freshly built `EFI/BOOT/BOOTX64.EFI`. The current UEFI binary lives in `bootloader/uefi/`, builds automatically with `make`, and simply prints `Hello bootloader` via the firmware console so we can verify the toolchain and disk layout in OVMF before implementing the full loader hand-off. A `startup.nsh` script is also dropped at the ESP root; on the first boot it registers a persistent `Boot####` entry via `bcfg` and chains into `\EFI\BOOT\BOOTX64.EFI`, so subsequent boots jump straight into the loader with no shell countdown.

## Install development environment
You have to install a cross compiler, and a cross debugger. `gcc` and `gdb`. The image builder also depends on `mtools` (for `mcopy`/`mmd`) and `mkfs.fat` (usually provided by the `dosfstools` package). The UEFI build needs a linker capable of `-m i386pep`; Ubuntu's `/usr/bin/ld` already supports this, but installing `lld` gives you an alternate linker you can select via `UEFI_LD`.

### Linux
You have to install first some libraries.
```bash
# Ubuntu/Debian-based Distributions
sudo apt-get install build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo mtools dosfstools ovmf lld
# Fedora/RHEL-based Distributions
sudo dnf install @development-tools bison flex gmp-devel mpfr-devel libmpc-devel texinfo mtools dosfstools edk2-ovmf lld
# Arch Linux
sudo pacman -S base-devel bison flex gmp mpfr libmpc texinfo mtools dosfstools edk2-ovmf lld
```

Then you have to go to a custom folder or the `Downloads` folder:

```bash
# Install binutils source code
wget https://ftp.gnu.org/gnu/binutils/binutils-2.36.1.tar.xz
# Install gcc source code
wget https://ftp.gnu.org/gnu/gcc/gcc-10.2.0/gcc-10.2.0.tar.xz
# Install gdb source code
wget https://ftp.gnu.org/gnu/gdb/gdb-10.2.tar.xz
```

Now I recomment switching to the super-user `sudo su`, or just sudo these following commands:
> Note: this may take some time

> Note: install the version that suits you (either the i386-elf or i686-elf). For now, we're using the i368, but we're probably going to migrate later. 

```bash
# i386-elf

# Downloading i686-elf binutils
mkdir build-binutils
cd build-binutils
../binutils-2.36.1/configure --target=i386-elf --prefix=/usr/local/cross --disable-nls --disable-werror
make
sudo make install
cd..

# Downloading i686-elf gcc
mkdir build-gcc
cd build-gcc
../gcc-10.2.0/configure --target=i386-elf --prefix=/usr/local/cross --disable-nls --enable-languages=c --without-headers
make all-gcc
sudo make install-gcc
cd ..

# Downloading i686-elf gdb
mkdir build-gdb
cd build-gdb
../gdb-10.2/configure --target=i386-elf --prefix=/usr/local/cross --disable-werror
make
sudo make install
```
or/and
```bash
# i686-elf

# Downloading i686-elf binutils
mkdir build-binutils
cd build-binutils
../binutils-2.36.1/configure --target=i686-elf --prefix=/usr/local/cross --disable-nls --disable-werror
make
sudo make install
cd..

# Downloading i686-elf gcc
mkdir build-gcc
cd build-gcc
../gcc-10.2.0/configure --target=i686-elf --prefix=/usr/local/cross --disable-nls --enable-languages=c --without-headers
make all-gcc
sudo make install-gcc
cd ..

# Downloading i686-elf gdb
mkdir build-gdb
cd build-gdb
../gdb-10.2/configure --target=i686-elf --prefix=/usr/local/cross --disable-werror
make
sudo make install
```

Then you need to update your `$PATH`
```bash
export PATH="/usr/local/cross/bin:$PATH"
```
