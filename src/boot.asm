[BITS 16]
[ORG 0x7c00]

CODE_OFFSET equ 0x8
DATA_OFFSET equ 0x10

start:
    cli            ; Clear + Disables interupts

    mov ax, 0x00   ; Zero Out Registers on Boot Startup
    mov dx, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00 ; Stack Pointer, start at origin, grows downwards to 0x00
    
    sti            ; Enable interupts again

load_PM:           ; Load Protected Mode
    lgdt[gdt_descriptor]
    mov eax, cr0
    or al, 1       ; Set the control register's first bit to 1, turning on protected mode
    mov cr0, eax
    jmp CODE_OFFSET:PModeMain

[BITS 32]
PModeMain:
    mov ax, DATA_OFFSET
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov ss, ax
    mov gs, ax
    mov ebp, 0x9C00
    mov esp, ebp

    in al, 0x92
    or al, 2
    out 0x92, al

    jmp $


%include "src/gdt.asm"
times 510 - ($ - $$) db 0 ; Fill leftover space with 0
dw 0xAA55 ; Super Special Bootloader Signiture for BIOS to read

