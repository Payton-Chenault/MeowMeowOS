section .text
extern main
global _start

_start:
    ; [esp]      = argc
    ; [esp + 4]  = argv

    mov eax, [esp]          ; eax = argc
    mov ebx, [esp + 4]      ; ebx = argv pointer

    push ebx
    push eax
    call main

    mov ebx, eax            ; exit status
    mov eax, 2              ; SYS_RETURN
    int 0x80

hang:
    jmp hang