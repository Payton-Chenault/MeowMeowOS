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