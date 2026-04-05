section .text
global keyboard_isr_wrapper
global default_isr_wrapper

extern keyboard_isr
extern interrupt_dispatcher

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