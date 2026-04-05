; For Point of reference, here is all registers used in boot.asm:
; AX -> 16-bit Accumulator Register
; DX -> 16-bit Data Register
; ES -> 16-bit Extra Segment Register
; SS -> 16-bit Stack Segment Register
; SP -> 16-bit Stack Pointer Register
; SI -> 16-bit Source Register




[BITS 16]
[ORG 0x7c00]

start:
    cli            ; Clear + Disables interupts

    mov ax, 0x00   ; Zero Out Registers on Boot Startup
    mov dx, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00 ; Stack Pointer, start at origin, grows downwards to 0x00
    
    sti            ; Enable interupts again

    mov si, boot_msg
    call print_string

    cli
    hlt            ; Stop further CPU execution



boot_msg: db "Starting OS", 0 
%include "src/print.asm"

times 510 - ($ - $$) db 0 ; Fill leftover space with 0
dw 0xAA55 ; Super Special Bootloader Signiture for BIOS to read

