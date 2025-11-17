#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
BIN_DIR="${REPO_ROOT}/.bin"
BIOS_IMAGE="${BIN_DIR}/os-image.bin"
DISK_IMAGE="${BIN_DIR}/casseos.img"
ESP_IMAGE="${BIN_DIR}/esp-fat32.img"
UEFI_STUB="${BIN_DIR}/uefi-placeholder.bin"

SECTOR_SIZE=512
ESP_SIZE_MB=64
ESP_SECTORS=$((ESP_SIZE_MB * 1024 * 1024 / SECTOR_SIZE))
PARTITION_START_LBA=2048   # 1 MiB boundary
TOTAL_SECTORS=$((PARTITION_START_LBA + ESP_SECTORS))
TOTAL_BYTES=$((TOTAL_SECTORS * SECTOR_SIZE))

export DISK_IMAGE PARTITION_START_LBA ESP_SECTORS

command -v mkfs.fat >/dev/null || {
    echo "mkfs.fat not found. Please install dosfstools." >&2
    exit 1
}

command -v mcopy >/dev/null || {
    echo "mcopy (from mtools) not found. Please install mtools." >&2
    exit 1
}

command -v mmd >/dev/null || {
    echo "mmd (from mtools) not found. Please install mtools." >&2
    exit 1
}

mkdir -p "${BIN_DIR}"

if [[ ! -f "${BIOS_IMAGE}" ]]; then
    echo "Missing ${BIOS_IMAGE}. Run 'make os-image' first." >&2
    exit 1
fi

BIOS_SIZE_BYTES=$(stat -c%s "${BIOS_IMAGE}")
BIOS_SECTORS=$(((BIOS_SIZE_BYTES + SECTOR_SIZE - 1) / SECTOR_SIZE))

if (( BIOS_SECTORS >= PARTITION_START_LBA )); then
    echo "BIOS image uses ${BIOS_SECTORS} sectors but only ${PARTITION_START_LBA} are reserved." >&2
    echo "Increase PARTITION_START_LBA or shrink the BIOS payload." >&2
    exit 1
fi

truncate -s 0 "${DISK_IMAGE}"
truncate -s "${TOTAL_BYTES}" "${DISK_IMAGE}"

dd if="${BIOS_IMAGE}" of="${DISK_IMAGE}" conv=notrunc bs=${SECTOR_SIZE} status=none

# Build a throwaway UEFI stub (two-byte infinite loop). Not a valid PE/COFF image yet.
printf '\xEB\xFE' > "${UEFI_STUB}"

ESP_KIB=$((ESP_SECTORS / 2))
mkfs.fat -F 32 -n CASSEESP -C "${ESP_IMAGE}" "${ESP_KIB}" >/dev/null
mmd -i "${ESP_IMAGE}" ::/EFI
mmd -i "${ESP_IMAGE}" ::/EFI/BOOT
mcopy -i "${ESP_IMAGE}" "${UEFI_STUB}" ::/EFI/BOOT/BOOTX64.EFI >/dev/null

dd if="${ESP_IMAGE}" of="${DISK_IMAGE}" conv=notrunc bs=${SECTOR_SIZE} seek=${PARTITION_START_LBA} status=none

python3 - <<'PY'
import os
import struct

disk_path = os.environ["DISK_IMAGE"]
start_lba = int(os.environ["PARTITION_START_LBA"])
sectors = int(os.environ["ESP_SECTORS"])

status = 0x00  # leave inactive so BIOS still boots via LBA0 code
start_chs = bytes([0x00, 0x02, 0x00])
partition_type = 0x0C  # FAT32 LBA
end_chs = bytes([0xFE, 0xFF, 0xFF])
entry = bytes([status]) + start_chs + bytes([partition_type]) + end_chs
entry += struct.pack("<I", start_lba)
entry += struct.pack("<I", sectors)

with open(disk_path, "r+b") as fh:
    fh.seek(446)
    fh.write(entry)
PY

rm -f "${ESP_IMAGE}" "${UEFI_STUB}"

echo "Created ${DISK_IMAGE} with BIOS loader plus ${ESP_SIZE_MB}MB FAT32 ESP."
