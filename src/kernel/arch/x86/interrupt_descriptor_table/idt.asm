[bits 32]
extern interrupt_dispatcher
extern syscall_dispatcher
global idt_load
global syscall_isr_wrapper
global keyboard_isr_wrapper
global timer_isr_wrapper
global page_fault_isr_wrapper
global default_isr_wrapper

idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

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
    push 32
    call interrupt_dispatcher
    add esp, 4

    popa 
    iret
keyboard_isr_wrapper:
    pusha
    
    push 33 
    call interrupt_dispatcher
    add esp, 4
    
    popa
    iret
page_fault_isr_wrapper:
    pusha

    push 14
    call interrupt_dispatcher
    add esp, 4

    popa
    add esp, 4
    iret

default_isr_wrapper:
    pusha
    push 0
    call interrupt_dispatcher
    add esp, 4
    popa
    iret