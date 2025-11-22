# AHCI: A Straightforward Story

Imagine you have a SATA disk hanging off your machine and you want your own kernel to talk to it. AHCI is just the agreed‑upon way for the OS and the SATA controller to communicate. Under the jargon there are only a few ideas: find the controller, find a port with a disk, give the controller some buffers, and ask it to move sectors in and out of memory for you.

This document walks through that story from scratch, assuming you mostly know “PCI exists” and “MMIO exists”, but not much more.

---

## What AHCI actually is

On a PC, your SATA ports are not wired directly to the CPU. They go into a SATA controller, and that controller is on the PCI bus. AHCI is a specification that says:

- how the SATA controller should describe itself on PCI (class codes, BAR layout),
- how it exposes a block of memory‑mapped registers (ABAR),
- how many “ports” it can have and how those ports look,
- and how you, the OS, ask it to do work using DMA instead of poking legacy ATA I/O ports.

So when you “write an AHCI driver”, you are really doing three things:

1. Use PCI to find the AHCI controller and map its MMIO region.
2. For each port that has a disk, give the controller some memory buffers that it can use for commands and data.
3. Fill a small structure that says “please read/write these sectors” and let the controller DMA data into your buffer.

That is the whole game. Everything else is details.

---

## Step 1: finding the controller

The AHCI controller is just another PCI device. During your PCI scan you read the class code, subclass, and programming interface. An AHCI SATA controller will report:

- class = 0x01 (mass storage),
- subclass = 0x06 (SATA),
- prog‑if = 0x01 (AHCI mode).

Once you find a device with those values, you look at its BARs. Typically BAR5 is the memory‑mapped base address (ABAR) for the AHCI registers. You must treat that as MMIO: read and write with normal memory loads and stores, not with `in`/`out` port I/O.

Before doing DMA, you also flip on PCI bus mastering for that device in its PCI command register. That is effectively saying “this device is allowed to drive the bus and read/write system RAM.”

---

## Step 2: looking inside ABAR

ABAR points at a big struct of registers called the Host Bus Adapter (HBA) memory. Conceptually it has two layers:

- some “global” registers describing the controller itself,
- and an array of per‑port register blocks for up to 32 ports.

A particularly important global register is `PI` (Port Implemented). Each bit in `PI` says whether a given port exists. If `PI` has bit 0 and bit 1 set, you have ports 0 and 1 implemented.

Each port has its own registers, exposed as something like `ports[0]`, `ports[1]`, etc. Two of those per‑port registers are key when you are just starting:

- `SSTS` (SATA Status) tells you whether there is a device on the cable and whether the link is up. If the DET field equals 3 and the speed is non‑zero, there is a live connection.
- `SIG` (Signature) tells you what type of thing is attached: a normal SATA disk, an ATAPI device, a port multiplier, and so on. For a simple disk, you expect 0x00000101.

Once you have found a port where `SSTS` says “device present” and `SIG` says “SATA disk”, that is the port you will talk to.

---

## Step 3: stopping the port and handing it buffers

Each AHCI port has some internal state, including the idea of a “command engine” that fetches commands from memory and executes them. The port needs to know:

- where in RAM your command list lives,
- where in RAM it should drop incoming FIS packets,
- and for each command, where in RAM the data buffers are.

You must stop the port before changing those addresses. That means:

1. Clear the “start” and “FIS receive enable” bits in the per‑port command register (`PxCMD.ST` and `PxCMD.FRE`).
2. Wait until the hardware says it has actually stopped (`PxCMD.CR` and `PxCMD.FR` go to zero).

Only after that is it safe to program new memory pointers.

Then you allocate a few buffers in your kernel:

- a **command list**: an array of command headers, 1 KiB total, aligned to 1 KiB,
- a **received FIS** buffer: 256 bytes, aligned to 256 bytes,
- one or more **command tables**: each holds the FIS you send and a table of DMA segments (PRDT),
- and one or more **data buffers** where sectors will be read into or written from.

You point `PxCLB/CLBU` at the command list base and `PxFB/FBU` at the received FIS buffer. Now the controller knows where to look in RAM when you ask it to do something.

To bring the port back to life you set `PxCMD.FRE` and then `PxCMD.ST`. At this point, the port is ready but idle: it is waiting for you to describe a command in the command list and poke a bit that says “go.”

---

## Step 4: what a command looks like

Each entry in the command list is called a command header. The command header points to a command table. The command table contains:

- a small “command FIS” that describes what to do (which command, which LBA, how many sectors),
- an optional “ATAPI command” field (for CD/DVD devices),
- and an array of PRDT entries, each describing a chunk of RAM to read from or write to.

For a simple read:

1. You pick a free command slot (for example, slot 0) by checking `PxCI` (Command Issue). If bit 0 is clear, slot 0 is free.
2. You zero the command header, set the length of the command FIS (in DWORDs), tell it how many PRDT entries there are, and point it at the command table.
3. You zero the command table and fill in one PRDT entry pointing at your data buffer. You tell it how many bytes to transfer (number of sectors * 512, minus one, because of how the field is defined).
4. You build the command FIS. AHCI uses the “Register Host‑to‑Device” FIS type (0x27). You set a bit that says “this is a command”, fill in the ATA command code (for example, `IDENTIFY DEVICE` or `READ DMA EXT`), set the LBA and sector count fields, and set the “LBA mode” bit.

When everything is set up in memory, you clear the per‑port interrupt status register (`PxIS`) so any old errors do not confuse the next operation. Then you set the bit in `PxCI` that corresponds to your slot. That is the moment where you hand control to the controller: it reads the command header and table, talks to the disk over SATA, and reads/writes data via DMA into your buffers.

You wait by polling that same bit in `PxCI`. While it is 1, the command is still active. Once the controller finishes, it clears the bit. If `PxIS` shows an error flag, or the task file status bits indicate a problem, the command failed. Otherwise your data buffer now contains your sectors.

---

## Step 5: IDENTIFY – asking “what disk is this?”

The very first useful command is `IDENTIFY DEVICE` (ATA command code 0xEC). It tells the disk to describe itself in a 512‑byte block of data. You send it almost like any other command, but with a couple of simplifications:

- you don’t need to set an LBA, because IDENTIFY operates on the device as a whole,
- you ask for one sector (because IDENTIFY always returns 512 bytes),
- you use a DMA buffer big enough for 512 bytes and point the PRDT at it.

After the command completes, your buffer holds the IDENTIFY data. It looks dense, but two fields are immediately useful:

- the model string (words 27–46), where each 16‑bit word has its bytes swapped,
- the sector count: older drives use a 28‑bit value (words 60–61); newer drives also report a 48‑bit value in words 100–103.

From that you can build a string like “QEMU HARDDISK” and a 64‑bit “number of sectors” value. Now you know how big the disk is.

---

## Step 6: READ DMA EXT – finally getting sectors

Once you know the size of the disk, reading real data is just a more general version of what you did for IDENTIFY. You use the `READ DMA EXT` command (code 0x25):

- you set the 48‑bit LBA spread across six bytes in the command FIS,
- you set the sector count in two bytes (low and high),
- you point the PRDT at a data buffer large enough for `sector_count * 512` bytes,
- and you issue the command by setting the slot bit in `PxCI`, then poll until it clears.

If the command succeeds, your buffer contains the raw sectors from the disk. Sector 0 might be an MBR; a typical check is to look for the boot signature 0x55AA at the last two bytes. At that point, you are doing real disk I/O.

---

## Step 7: how this fits into your OS

Once you have an AHCI port that can identify the drive and read sectors, integrating it into the rest of your kernel is about layering:

- you wrap each SATA port in a small “block device” object that exposes “read N sectors at LBA X into this buffer,”
- you add a way to list them (for example, a shell command that prints device index, model, and size),
- and eventually you build a filesystem layer on top of that block interface (FAT, ext2, or something custom).

From there you can do higher‑level things: load binaries from disk, read configuration files, or mount multiple volumes. AHCI itself does not care about any of that; it just moves sectors.

---

## Step 8: debugging when it breaks

When something goes wrong, it is usually one of a small set of mistakes:

- the port was not properly stopped before you programmed `PxCLB` and `PxFB`,
- the command list or FIS buffer was not aligned as the spec requires,
- you forgot to enable PCI bus mastering, so DMA silently fails,
- you did not clear `PxIS`, and a previous error flag is making every command look broken,
- or you mis‑computed the PRDT byte count (it must be “bytes minus one”).

Sticking to a simple pattern helps: one port, one command slot, one PRDT, one sector, and polling. Once that works reliably in QEMU, you can make the driver smarter with multiple PRDT entries, multiple outstanding commands, and interrupt‑driven completion. But the basic recipe—find controller, pick port, hand it buffers, issue IDENTIFY, then READ DMA—is the same no matter how fancy you get later. 
