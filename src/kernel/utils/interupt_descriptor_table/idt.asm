section .text
global keyboard_isr_wrapper
global default_isr_wrapper

extern keyboard_isr
extern interupt_dispatcher

keyboard_isr_wrapper:
    pusha
    push 33
    call interupt_dispatcher
    add esp, 4
    popa
    iret

default_isr_wrapper:
    pusha
    push 0
    call interupt_dispatcher
    add esp, 4
    popa
    iret