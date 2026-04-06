#ifndef IDT_H
#define IDT_H

#include <stdint.h>
#include <stddef.h>
#include "../../intf/ports/IO.h"
#include "../logging//logger.h"

struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t always_zero;
    uint8_t flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base; 
} __attribute__((packed));

void register_interrupt_handler(uint8_t vector, void (*handler)(void));

/**
 * @brief Initializes the Interrupt Descriptor Table
 * 
 */
void idt_initialize(void);

static inline void enable_interrupts(void) {
    __asm__ volatile("sti"); 
}

static inline void disable_interrupts(void) {
    __asm__ volatile("cli"); 
}

static inline void wait_for_interrupt(void) {
    __asm__ volatile("hlt");
}

#define IRQ0_TIMER      0
#define IRQ1_KEYBOARD   1
#define IRQ2_CASCADE    2
#define IRQ3_COM2       3
#define IRQ4_COM1       4
#define IRQ5_LPT2       5
#define IRQ6_FLOPPY     6
#define IRQ7_LPT1       7
#define IRQ8_CMOS       8
#define IRQ9_ACPI       9
#define IRQ10_SCSI      10
#define IRQ11_USB       11
#define IRQ12_MOUSE     12
#define IRQ13_FPU       13
#define IRQ14_ATA1      14
#define IRQ15_ATA2      15

#define IRQ_BASE_OFFSET 32
#define IRQ_TO_VECTOR(irq) (IRQ_BASE_OFFSET + (irq))

#define KEYBOARD_INTERRUPT_VECTOR IRQ_TO_VECTOR(IRQ1_KEYBOARD)
#endif