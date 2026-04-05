[BITS 16]
[ORG 0x7c00]

CODE_OFFSET     equ 0x08
DATA_OFFSET     equ 0x10

KERNEL_LOAD_SEG equ 0x1000       ; Segment where we load the kernel via int 0x13

start:
    cli                     ; Disable interrupts

    ; Zero out registers and setup stack
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    sti                     ; Enable interrupts

    ; Enable A20 line
    in al, 0x92
    or al, 2
    out 0x92, al

    ; Load kernel from disk
    mov ax, KERNEL_LOAD_SEG
    mov es, ax
    xor bx, bx
    mov ah, 0x02
    mov al, 32               ; Number of sectors to read
    mov ch, 0x00             ; Cylinder
    mov cl, 0x02             ; Sector (starting from 2, since sector 1 is bootloader)
    mov dh, 0x00             ; Head
    mov dl, 0x80             ; Drive (first HDD)
    int 0x13
    jc disk_read_error
    jmp lgdt_setup           ; Jump to GDT setup on success

disk_read_error:
    cli
    hlt
    jmp disk_read_error

lgdt_setup:
    cli                     ; Disable interrupts before mode switch
    xor ax, ax
    mov ds, ax
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump to 32-bit code segment
    jmp CODE_OFFSET:pmode_start

; GDT Data Section
align 16
gdt_start:
    ; Null descriptor
    dd 0x0
    dd 0x0

    ; Code segment descriptor
    dw 0xFFFF               ; Limit 0-15
    dw 0x0000               ; Base 0-15
    db 0x00                 ; Base 16-23
    db 10011010b            ; Access byte (Present, Ring0, Code, Non-conforming, Readable)
    db 11001111b            ; Flags (4KB granularity, 32-bit) + Limit 16-19
    db 0x00                 ; Base 24-31

    ; Data segment descriptor
    dw 0xFFFF               ; Limit 0-15
    dw 0x0000               ; Base 0-15
    db 0x00                 ; Base 16-23
    db 10010010b            ; Access byte (Present, Ring0, Data, Expand-up, Writable)
    db 11001111b            ; Flags (4KB granularity, 32-bit) + Limit 16-19
    db 0x00                 ; Base 24-31
gdt_end:

align 4
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start  ; Add ORG base to get physical address

[BITS 32]
pmode_start:
    ; Setup segments
    mov ax, DATA_OFFSET
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov ss, ax
    mov gs, ax

    ; Setup stack (far away from kernel)
    mov ebp, 0x9C00
    mov esp, ebp

    ; Far jump to kernel entry point
    jmp dword CODE_OFFSET:0x10000

; Boot sector signature
times 510-($-$$) db 0
dw 0xAA55