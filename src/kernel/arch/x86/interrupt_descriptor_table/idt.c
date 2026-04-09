#include "idt.h"
#include "../../../drivers/ports/IO.h"
#include "../../../utils/logging/logger.h"

#define MODULE "IDT"

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20

#define ICW1_INIT    0x11
#define ICW4_8086    0x01

#define IDT_GATE_32BIT_INT  0x8E
#define KERNEL_CS           0x08 

extern void keyboard_isr_wrapper(void);
extern void timer_isr_wrapper(void);
extern void page_fault_isr_wrapper(void);
extern void default_isr_wrapper(void);
extern void syscall_isr_wrapper(void);

static idt_entry_t idt[256];
static idt_ptr_t   idtp;

static bool (*handlers[256])(void);

/**
 * @brief Helper to configure a single IDT entry
 */
static void set_idt_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags) {
    idt[num].base_low    = (base & 0xFFFF);
    idt[num].base_high   = (base >> 16) & 0xFFFF;
    idt[num].sel         = selector;
    idt[num].always_zero = 0;
    idt[num].flags       = flags;
}

/**
 * @brief Remaps the PIC so IRQs don't overlap with CPU exceptions
 */
static void pic_configure(uint8_t master_offset, uint8_t slave_offset) {
    outb(PIC1_COMMAND, ICW1_INIT);
    outb(PIC2_COMMAND, ICW1_INIT);

    outb(PIC1_DATA, master_offset);
    outb(PIC2_DATA, slave_offset);

    outb(PIC1_DATA, 4);
    outb(PIC2_DATA, 2);

    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    outb(PIC1_DATA, 0xFC); 
    outb(PIC2_DATA, 0xFF); 
}


void register_interrupt_handler(uint8_t vector, bool (*handler)(void)) {
    handlers[vector] = handler;
    log_debug(MODULE, "Registered handler for vector: 0x%x", vector);
}

void idt_initialize(void) {
    for (int i = 0; i < 256; i++) {
        handlers[i] = NULL;
    }

    idtp.limit = (sizeof(idt_entry_t) * 256) - 1;
    idtp.base  = (uint32_t)&idt;

    for (int i = 0; i < 256; i++) {
        set_idt_gate(i, (uint32_t)default_isr_wrapper, KERNEL_CS, IDT_GATE_32BIT_INT);
    }

    set_idt_gate(TIMER_INTERRUPT_VECTOR,    (uint32_t)timer_isr_wrapper,    KERNEL_CS, 0x8E);
    set_idt_gate(KEYBOARD_INTERRUPT_VECTOR, (uint32_t)keyboard_isr_wrapper, KERNEL_CS, 0x8E);
    set_idt_gate(EXCEPTION_PAGE_FAULT,      (uint32_t)page_fault_isr_wrapper, KERNEL_CS, 0x8E);
    set_idt_gate(SYSCALL_INTERUPT_VECTOR,      (uint32_t)syscall_isr_wrapper, KERNEL_CS, 0xEE);


    pic_configure(0x20, 0x28);

    __asm__ volatile ("lidt %0" : : "m"(idtp));

    log_info(MODULE, "IDT and PIC Initialized");
}

/**
 * @brief Dispatches interrupts from assembly to C handlers
 */
void interrupt_dispatcher(uint32_t vector) {
    if (handlers[vector] != NULL) {
        bool request_panic = handlers[vector]();
        if (request_panic) {
            log_error(MODULE, "Critical failure in handler 0x%x", vector);
            kpanic("Interrupt handler requested immediate system halt");
        }
    } 
    else if (vector < 32) {
        log_error(MODULE, "Unhandled Processor Exception: 0x%x", vector);
        kpanic("Processor Exception (Kernel Halt)");
    }
    else {
        log_warning(MODULE, "No handler for IRQ vector: 0x%x", vector);
    }


    if (vector >= 40) {
        outb(PIC2_COMMAND, PIC_EOI); 
    }
    outb(PIC1_COMMAND, PIC_EOI);
}