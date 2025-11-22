# Block Driver Roadmap

This document tracks the long-term plan for introducing block-device support so the kernel can read files from disks and eventually load binary commands.

## Phase 1: Hardware Discovery & Block Layer
1. Enumerate PCI storage controllers and capture their BARs/capabilities in a minimal `drivers/block/` hierarchy (starting with AHCI for SATA disks).
2. Implement a generic block-device interface (`struct block_device { read, write, sector_size, capacity }`) that the kernel and future filesystems can consume.
3. Add boot-time init hooks so block drivers register themselves after PCI scan but before the shell starts; expose a debugging shell command to list detected drives.

## Phase 2: Reliable I/O & Buffer Management
1. Support DMA setup (PRDTs for AHCI) and interrupt-driven completion to avoid busy waiting during reads.
2. Provide a temporary bounce-buffer allocator to align sector transfers until a full pager/VM system exists.
3. Create simple stress tests (e.g., repeatedly reading the first N sectors) callable from the shell to validate transfers.

## Phase 3: Filesystem Foundations
1. Pick an initial filesystem (likely FAT12/16/32 for compatibility with existing ISO tooling) and implement a read-only parser that consumes the block layer.
2. Integrate the filesystem mount info into a kernel-wide VFS stub so future filesystems (EXT2, custom FS) can plug in.
3. Extend the shell to load text scripts or binary command stubs from the mounted filesystem to prove the pipeline.

## Phase 4: Toward Userland & Advanced Features
1. Add write support and caching once memory management matures, ensuring the block layer can queue multiple outstanding requests.
2. Move the command shell into user space, using the block + filesystem layers to load real binaries.
3. Evaluate additional controllers (NVMe, USB mass storage) to broaden hardware coverage under the same block interface.
