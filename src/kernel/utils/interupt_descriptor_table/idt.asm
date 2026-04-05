[bits 32]
extern interrupt_dispatcher
global idt_load
global keyboard_isr_wrapper
global default_isr_wrapper

idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

keyboard_isr_wrapper:
    pusha
    
    push 33 
    call interrupt_dispatcher
    add esp, 4
    
    popa
    iret

default_isr_wrapper:
    pusha
    push 0
    call interrupt_dispatcher
    add esp, 4
    popa
    iret