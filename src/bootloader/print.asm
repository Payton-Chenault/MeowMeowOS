; For reference purposes, here are all the registers used in print.asm:
; AH -> 8-bit High Accumulator Register, Used for Specific BIOS Interuption Routine
; BH -> 8-bit High Base Register
; BL -> 8-bit Low Base Register
; AL -> 8-bit Low Accumulator Register

print_char:
    mov ah, 0x0E   ; Sets the video service request (In this case Write Char is TTY Mode)
    mov bh, 0x00   ; Display on page 0
    mov bl, 0x07   ; White on Black Color
    int 0x10       ; Interupt the BIOS for a video service
    ret

print_string:
    .print_loop:
        lodsb     ; Loads byte at ds:si to AL register and increments SI
        cmp al, 0 ; Checks to see if the null pointer is present
        je .done
        call print_char
        jmp .print_loop
    .done:
        ret
