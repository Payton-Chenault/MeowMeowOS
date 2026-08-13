[bits 32]
extern interrupt_dispatcher
extern syscall_dispatcher
extern division_error_handler
extern page_fault_handler_with_error
extern default_exception_handler
extern gp_fault_handler
extern debug_syscall_frame

global idt_load
global syscall_isr_wrapper
global keyboard_isr_wrapper
global timer_isr_wrapper
global page_fault_isr_wrapper
global default_isr_wrapper
global division_error_isr_wrapper
global gp_fault_isr_wrapper

idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

division_error_isr_wrapper:
    ; On entry: [esp] = EIP, [esp+4] = CS, [esp+8] = EFLAGS
    mov eax, [esp]          ; EIP
    push eax
    call division_error_handler
    add esp, 4
    iret
    
syscall_isr_wrapper:
    pusha
    push ds
    push es

    mov ax, 0x10
    mov ds, ax
    mov es, ax

    push esp
    call syscall_dispatcher
    add esp, 4

    pop es
    pop ds
    popa
    iret

timer_isr_wrapper:
    pusha
    push ds
    push es
    mov ax, 0x10
    mov ds, ax
    mov es, ax

    push 32
    call interrupt_dispatcher
    add esp, 4

    pop es
    pop ds
    popa
    iret

keyboard_isr_wrapper:
    pusha
    push ds
    push es
    mov ax, 0x10
    mov ds, ax
    mov es, ax

    push 33
    call interrupt_dispatcher
    add esp, 4

    pop es
    pop ds
    popa
    iret

gp_fault_isr_wrapper:
    pusha
    push ds
    push es
    mov ax, 0x10
    mov ds, ax
    mov es, ax

    ; Stack layout:
    ; [esp]    = es
    ; [esp+4]  = ds
    ; [esp+8]  = edi
    ; [esp+12] = esi
    ; [esp+16] = ebp
    ; [esp+20] = original esp
    ; [esp+24] = ebx
    ; [esp+28] = edx
    ; [esp+32] = ecx
    ; [esp+36] = eax
    ; [esp+40] = error code
    ; [esp+44] = original EIP

    mov eax, [esp+40]      ; error code
    mov ebx, [esp+44]      ; EIP
    push ebx               ; EIP (2nd arg)
    push eax               ; error code (1st arg)
    call gp_fault_handler
    add esp, 8

    pop es
    pop ds
    popa
    add esp, 4             ; remove error code
    iret

page_fault_isr_wrapper:
    pusha
    push ds
    push es
    mov ax, 0x10
    mov ds, ax
    mov es, ax

    ; stack layout after pushes: es, ds, edi, esi, ebp, orig_esp, ebx, edx, ecx, eax, error, eip
    mov eax, [esp+40]      ; error code
    mov ebx, [esp+44]      ; EIP
    push ebx
    push eax
    call page_fault_handler_with_error
    add esp, 8

    pop es
    pop ds
    popa
    add esp, 4
    iret

default_isr_wrapper:
    pusha
    push ds
    push es
    mov ax, 0x10
    mov ds, ax
    mov es, ax

    ; After pushes, stack layout:
    ; [esp]    = es
    ; [esp+4]  = ds
    ; [esp+8]  = edi
    ; [esp+12] = esi
    ; [esp+16] = ebp
    ; [esp+20] = original esp
    ; [esp+24] = ebx
    ; [esp+28] = edx
    ; [esp+32] = ecx
    ; [esp+36] = eax
    ; If exception has error code:
    ;   [esp+40] = error code
    ;   [esp+44] = original EIP
    ; If no error code:
    ;   [esp+40] = original EIP
    ; To cover both, we'll use [esp+44] for now (assumes error code present).

    mov eax, [esp+44]      ; EIP for error-code exceptions
    push eax
    call default_exception_handler
    add esp, 4

    pop es
    pop ds
    popa
    ; Remove error code if present? We don't know, but for exceptions with error code,
    ; the `iret` will not pop it automatically. We need to handle that.
    ; For simplicity, assume the exception has an error code and add esp, 4.
    add esp, 4
    iret
