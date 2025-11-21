ENTRY(_start)

MEMORY
{
    RAM (xrw) : ORIGIN = 0x80000, LENGTH = 4M
}

SECTIONS
{
    .text : {
        *(.text*)
    } > RAM

    /* Read-only data (rodata) section */
    .rodata : {
        *(.rodata*)
    } > RAM

    /* Initialized data (data) section */
    .data : {
        *(.data*)
    } > RAM

    .bootinfo : {
        KEEP(*(.bootinfo))
    } > RAM

    /* Uninitialized data (bss) section */
    .bss : {
        *(.bss*)
    } > RAM
}
