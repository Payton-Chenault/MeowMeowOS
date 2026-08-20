[bits 32]
global switch_to_task
global switch_to_user_task

; Switch to a kernel task
; C declaration: void switch_to_task(uint32_t* old_esp, uint32_t new_esp, uint32_t new_cr3);
switch_to_task:
    ; Save current kernel context
    push ebp
    push ebx
    push esi
    push edi

    ; Save old esp
    mov eax, [esp + 20]      ; old_esp pointer
    mov [eax], esp

    ; Load new context
    mov ecx, [esp + 24]      ; new_esp
    mov edx, [esp + 28]      ; new_cr3

    ; Switch CR3 if needed
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

    ; Ensure interrupts are enabled for the task being switched to
    sti
    ret

; Switch to a user task
; C declaration: void switch_to_user_task(uint32_t* old_esp, uint32_t new_esp, uint32_t new_cr3);
; new_esp should point to an iret frame: [SS, ESP, EFLAGS, CS, EIP]
switch_to_user_task:
    ; Save current kernel context
    push ebp
    push ebx
    push esi
    push edi

    ; Save old esp
    mov eax, [esp + 20]      ; old_esp pointer
    mov [eax], esp

    ; Load new esp and cr3
    mov ecx, [esp + 24]      ; new_esp (points to iret frame)
    mov edx, [esp + 28]      ; new_cr3

    ; ALWAYS reload CR3 to flush TLB
    mov cr3, edx

    ; Switch to new stack (the iret frame)
    mov esp, ecx

    ; Enable interrupts and enter user mode
    sti
    iret
