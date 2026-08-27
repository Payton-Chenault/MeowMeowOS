[bits 32]
global switch_to_task
global return_to_user_mode

switch_to_task:

    push ebp
    push ebx
    push esi
    push edi

    mov eax, [esp + 20]      ; old_esp pointer
    mov [eax], esp

    mov ecx, [esp + 24]      ; new_esp
    mov edx, [esp + 28]      ; new_cr3

    mov eax, cr3
    cmp eax, edx
    je .skip_cr3
    mov cr3, edx
.skip_cr3:

    mov esp, ecx             ; switch to new kernel stack

    pop edi
    pop esi
    pop ebx
    pop ebp

    sti
    ret

return_to_user_mode:
    iret