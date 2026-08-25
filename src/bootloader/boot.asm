[BITS 16]
[ORG 0x7c00]

jmp short start
nop

times 59 db 0

CODE_OFFSET     equ 0x08
DATA_OFFSET     equ 0x10

KERNEL_LOAD_SEG equ 0x1000       ; Segment where we load the kernel

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

    ; Load kernel from disk using LBA Extended Read
    mov dl, 0x80             ; Drive (first HDD)
    mov cx, 300              ; Number of sectors to read (300 * 512 = ~150 KB)
    mov ax, KERNEL_LOAD_SEG  ; Starting memory segment
    
load_loop:
    push ax
    push cx
    
    mov [dap_segment], ax    ; Update the DAP with the current segment
    
    mov ah, 0x42             ; Extended Read function
    mov si, dap              ; Pointer to the Disk Address Packet
    int 0x13
    jc disk_read_error
    
    ; Increment the LBA to read the next sector
    mov eax, dword [dap_lba]
    inc eax
    mov dword [dap_lba], eax
    
    ; Advance the memory pointer by 512 bytes (0x20 in segment notation)
    pop cx
    pop ax
    add ax, 0x20
    loop load_loop
    
    jmp get_memory_map

; Disk Address Packet (DAP) Structure
align 4
dap:
    db 0x10                  ; Size of DAP
    db 0                     ; Unused
    dw 1                     ; Read 1 sector at a time
    dw 0x0000                ; Target offset (always 0)
dap_segment:
    dw KERNEL_LOAD_SEG       ; Target segment (dynamically updated)
dap_lba:
    dd 1                     ; Start LBA (Sector 1, skipping bootloader)
    dd 0                     ; Upper 32-bits of LBA

get_memory_map:
    xor ax, ax
    mov es, ax
    
    mov di, 0x9004
    xor ebx, ebx
    xor bp, bp
    mov edx, 0x534D4150

.mmap_loop:
    mov eax, 0xE820
    mov [es:di + 20], dword 1
    mov ecx, 24
    int 0x15
    jc .mmap_done

    mov edx, 0x534D4150
    cmp eax, edx
    jne .mmap_error

    mov eax, [es:di + 8]
    or eax, [es:di + 12]

    jz .skip_entry

    inc bp
    add di, 24

.skip_entry:
    test ebx, ebx
    jnz .mmap_loop

.mmap_done:
    mov [0x9000], bp
    
    ; Setup VBE Linear Framebuffer
    mov ax, 0x4F01
    mov cx, 0x0118       ; Request 1024x768x32 Mode Info
    mov di, 0x8000       ; Store Mode Info Block safely at 0x8000
    int 0x10
    cmp ax, 0x004F
    jne lgdt_setup       ; Skip setting mode if unsupported by hardware
    
    mov ax, 0x4F02
    mov bx, 0x4118       ; Set mode 0x0118 with Linear Framebuffer (bit 14 set)
    int 0x10
    jmp lgdt_setup

.mmap_error:
    mov dword [0x9000], 0xFFFFFFFF
    jmp lgdt_setup

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
    mov ebp, 0x90000
    mov esp, ebp

    ; Far jump to kernel entry point
    jmp dword CODE_OFFSET:0x10000

; Boot sector signature
times 510-($-$$) db 0
dw 0xAA55