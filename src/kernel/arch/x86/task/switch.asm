[BITS 32]
global switch_to_task
global enter_ring3

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

enter_ring3:
    mov eax, [esp+4] ; EIP (Entry Point of the ELF)
    mov ebx, [esp+8] ; ESP (The new Ring 3 Stack)

    mov cx, 0x23
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx

    push 0x23
    push ebx
    
    pushf
    pop ecx
    or ecx, 0x200
    push ecx
    
    push 0x1B
    push eax

    iret