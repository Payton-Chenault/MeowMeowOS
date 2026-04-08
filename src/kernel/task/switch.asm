[BITS 32]
global switch_to_task

switch_to_task:
    ; Save old state
    push ebp
    push ebx
    push esi
    push edi

    mov eax, [esp + 20]
    mov [eax], esp

    ; Load new state
    mov ecx, [esp + 24]     ; new_esp
    mov edx, [esp + 28]     ; new_cr3

    mov eax, cr3
    cmp eax, edx
    je .skip_cr3
    mov cr3, edx
.skip_cr3:

    mov esp, ecx

    pop edi
    pop esi
    pop ebx
    pop ebp

    sti 
    ret