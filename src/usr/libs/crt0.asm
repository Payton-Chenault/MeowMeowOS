; crt0.asm
global _start
global _fini        
extern main
extern exit

; Declare linker script symbols as external
extern __init_array_start
extern __init_array_end
extern __fini_array_start
extern __fini_array_end

section .text
_start:
    call _init

    mov eax, [esp]
    mov ebx, [esp + 4]
    push ebx
    push eax
    call main
    add esp, 8

    ; exit with return code
    push eax
    call exit
    ; Should not return

; Constructor/destructor support
section .text
_init:
    push ebp
    mov ebp, esp
    push ebx
    mov ebx, __init_array_start
    jmp .loop_check
.loop:
    ; Call function at [ebx]
    push eax
    call [ebx]
    pop eax
    add ebx, 4
.loop_check:
    cmp ebx, __init_array_end
    jb .loop
    pop ebx
    mov esp, ebp
    pop ebp
    ret

_fini:
    push ebp
    mov ebp, esp
    push ebx
    mov ebx, __fini_array_end
    sub ebx, 4
    jmp .fini_loop_check
.fini_loop:
    call [ebx]
    sub ebx, 4
.fini_loop_check:
    cmp ebx, __fini_array_start
    jae .fini_loop
    pop ebx
    mov esp, ebp
    pop ebp
    ret