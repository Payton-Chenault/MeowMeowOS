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

global irq9_isr_wrapper
global irq10_isr_wrapper
global irq11_isr_wrapper
global irq12_isr_wrapper

idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

division_error_isr_wrapper:
    push 0                  ; Dummy error code for stack alignment
    pusha
    push ds
    push es
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    push esp                ; Pass pointer to cpu_registers_t
    call division_error_handler
    add esp, 4              ; Clean up passed pointer
    pop es
    pop ds
    popa
    add esp, 4              ; Pop dummy error code
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

irq9_isr_wrapper:
    pusha
    push ds
    push es
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    push 41
    call interrupt_dispatcher
    add esp, 4
    pop es
    pop ds
    popa
    iret

irq10_isr_wrapper:
    pusha
    push ds
    push es
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    push 42
    call interrupt_dispatcher
    add esp, 4
    pop es
    pop ds
    popa
    iret

irq11_isr_wrapper:
    pusha
    push ds
    push es
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    push 43
    call interrupt_dispatcher
    add esp, 4
    pop es
    pop ds
    popa
    iret

irq12_isr_wrapper:
    pusha
    push ds
    push es
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    push 44
    call interrupt_dispatcher
    add esp, 4
    pop es
    pop ds
    popa
    iret

gp_fault_isr_wrapper:
    ; CPU automatically pushed the error code
    pusha
    push ds
    push es
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    push esp                ; Pass pointer to cpu_registers_t
    call gp_fault_handler
    add esp, 4
    pop es
    pop ds
    popa
    add esp, 4              ; Pop error code pushed by CPU
    iret

page_fault_isr_wrapper:
    ; CPU automatically pushed the error code
    pusha
    push ds
    push es
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    push esp                ; Pass pointer to cpu_registers_t
    call page_fault_handler_with_error
    add esp, 4
    pop es
    pop ds
    popa
    add esp, 4              ; Pop error code pushed by CPU
    iret

default_isr_wrapper:
    push 0                  ; Assume no error code for default catch-all
    pusha
    push ds
    push es
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    push esp                ; Pass pointer to cpu_registers_t
    call default_exception_handler
    add esp, 4
    pop es
    pop ds
    popa
    add esp, 4              ; Pop dummy error code
    iret