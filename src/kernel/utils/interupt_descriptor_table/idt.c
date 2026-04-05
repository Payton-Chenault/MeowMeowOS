#include "idt.h"
#include <stdint.h>

#define MODULE "IDT"
extern void keyboard_isr_wrapper(void);
extern void default_isr_wrapper(void);

static struct idt_entry idt[256];
static struct idt_ptr idtp;

static void (*handlers[256])(void);

static void set_idt_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags) {
    idt[num].base_low = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel = selector;
    idt[num].always_zero = 0;
    idt[num].flags = flags;
}

void register_interrupt_handler(uint8_t vector, void (*handler)(void)) {
    handlers[vector] = handler;
}

void init_idt(void) {
    for (int i =0; i < 256; i++) {
        handlers[i] = NULL;
    }

    idtp.limit = sizeof(struct idt_entry) * 256 - 1;
    idtp.base = (uint32_t)&idt;

    for (int i = 0; i < 256; i++) {
        set_idt_gate(i, (uint32_t)default_isr_wrapper, 0x08, 0x8E);
    }

    set_idt_gate(KEYBOARD_INTERRUPT_VECTOR, (uint32_t)keyboard_isr_wrapper, 0x08, 0x0E);

    outb(0x20, 0x11);  // Send ICW1 to master PIC
    outb(0xA0, 0x11);  // Send ICW1 to slave PIC
    outb(0x21, 0x20);  // ICW2: master PIC vector offset (32)
    outb(0xA1, 0x28);  // ICW2: slave PIC vector offset (40)
    outb(0x21, 0x04);  // ICW3: tell master PIC there's a slave at IRQ2
    outb(0xA1, 0x02);  // ICW3: tell slave PIC its cascade identity
    outb(0x21, 0x01);  // ICW4: set x86 mode
    outb(0xA1, 0x01);  // ICW4: set x86 mode
    
    outb(0x21, 0xFF);  // Mask all on master
    outb(0xA1, 0xFF);  // Mask all on slave

    __asm__ volatile ("lidt %0" : : "m"(idtp));
}

    void interrupt_dispatcher(uint32_t vector) {
        if (handlers[vector] != NULL) {
            handlers[vector]();
        } else {
            log_error(MODULE, "Unhandled Exception: 0x%x", vector);
        }

        if (vector >= 40) {
            outb(0xA0, 0x20);
        } 

        outb(0x20, 0x20);
    }
