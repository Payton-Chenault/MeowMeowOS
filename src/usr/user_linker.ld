ENTRY(_start)

SECTIONS
{
    . = 0x08048000 + SIZEOF_HEADERS;

    .text : ALIGN(4K) {
        *(.text*)
    }

    .rodata : ALIGN(4K) {
        *(.rodata*)
    }

    .init_array : ALIGN(4) {
        __init_array_start = .;
        KEEP(*(.init_array))
        KEEP(*(.ctors))       /* legacy */
        __init_array_end = .;
    }

    .fini_array : ALIGN(4) {
        __fini_array_start = .;
        KEEP(*(.fini_array))
        KEEP(*(.dtors))       /* legacy */
        __fini_array_end = .;
    }

    .description : ALIGN(4K) {
        KEEP(*(.description))
    }

    .data : ALIGN(4K) {
        *(.data*)
    }

    .bss : ALIGN(4K) {
        *(COMMON)
        *(.bss*)
    }
}