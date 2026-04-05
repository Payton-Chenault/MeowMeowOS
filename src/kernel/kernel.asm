[BITS 32]
global _start
extern kernel_main
_start:
    call kernel_main
    cli
    hlt
    jmp $
times 512 - ($ - $$) db 0