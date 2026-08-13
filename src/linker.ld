ENTRY(_start)

SECTIONS
{
    /* Load address for the kernel (64 KB mark) */
    . = 0x10000;

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