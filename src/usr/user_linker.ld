ENTRY(_start)

SECTIONS
{
    /* Start after the ELF headers to avoid executing them */
    . = 0x08048000 + SIZEOF_HEADERS;

    .text : ALIGN(4K) {
        *(.text*)
    }

    .rodata : ALIGN(4K) {
        *(.rodata*)
    }

    .data : ALIGN(4K) {
        *(.data*)
    }

    .bss : ALIGN(4K) {
        *(COMMON)
        *(.bss*)
    }
}