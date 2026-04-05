[BITS 16]
[ORG 0x7c00]

CODE_OFFSET equ 0x8
DATA_OFFSET equ 0x10

KERNEL_LOAD_SEG equ 0x1000

KERNEL_START_ADDR equ 0x100000
_start:
    cli            ; Clear + Disables interupts

    mov ax, 0x00   ; Zero Out Registers on Boot Startup
    mov dx, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00 ; Stack Pointer, start at origin, grows downwards to 0x00
    

    sti            ; Enable interupts again

; Load Kernel
    mov bx, KERNEL_LOAD_SEG
    mov dh, 0x00
    mov cl, 0x02
    mov ch, 0x00
    mov ah, 0x02
    mov al, 8
    int 0x13

    jc disk_read_error


load_PM:           ; Load Protected Mode
    cli
    lgdt[gdt_descriptor]
    mov eax, cr0
    or al, 1       ; Set the control register's first bit to 1, turning on protected mode
    mov cr0, eax
    jmp CODE_OFFSET:PModeMain

disk_read_error:
    hlt

%include "src/bootloader/gdt.asm"

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

    jmp CODE_OFFSET:KERNEL_START_ADDR

times 510 - ($ - $$) db 0 ; Fill leftover space with 0
dw 0xAA55 ; Super Special Bootloader Signiture for BIOS to read

