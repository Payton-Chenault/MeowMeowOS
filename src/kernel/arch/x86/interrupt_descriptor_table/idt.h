#ifndef IDT_H
#define IDT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/** * IDT Gate Type Attributes 
 * P (1b) | DPL (2b) | S (1b) | Type (4b)
 */
#define IDT_FLAG_PRESENT     0x80
#define IDT_FLAG_RING0       0x00
#define IDT_FLAG_RING3       0x60
#define IDT_FLAG_GATE_32INT  0x0E // 32-bit Interrupt Gate
#define IDT_FLAG_GATE_32TRP  0x0F // 32-bit Trap Gate

/* Standard Gate for Ring 0 Interrupts: 1 (P) 00 (DPL) 0 (S) 1110 (Type) = 0x8E */
#define IDT_ATTR_INT_GATE_R0 (IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32INT)

/**
 * @brief Represents a single entry in the Interrupt Descriptor Table
 */
typedef struct {
    uint16_t base_low;    // Lower 16 bits of the ISR address
    uint16_t sel;         // Kernel segment selector (usually 0x08)
    uint8_t  always_zero; // Reserved, must be 0
    uint8_t  flags;       // Attributes (Type, Privilege Level, Present bit)
    uint16_t base_high;   // Upper 16 bits of the ISR address
} __attribute__((packed)) idt_entry_t;

/**
 * @brief The register structure loaded into the CPU via 'lidt'
 */
typedef struct {
    uint16_t limit;       // Size of the IDT table - 1
    uint32_t base;        // Starting address of the idt_entry_t array
} __attribute__((packed)) idt_ptr_t;

/* --- X86 Processor Exceptions (0-31) --- */
#define EXCEPTION_DIV_BY_ZERO         0
#define EXCEPTION_DEBUG               1
#define EXCEPTION_NMI                 2
#define EXCEPTION_BREAKPOINT          3
#define EXCEPTION_OVERFLOW            4
#define EXCEPTION_BOUND_RANGE         5
#define EXCEPTION_INVALID_OPCODE      6
#define EXCEPTION_DEV_NOT_AVAIL       7
#define EXCEPTION_DOUBLE_FAULT        8
#define EXCEPTION_COPROC_SEG_OVERRUN  9
#define EXCEPTION_INVALID_TSS         10
#define EXCEPTION_SEG_NOT_PRESENT     11
#define EXCEPTION_STACK_FAULT         12
#define EXCEPTION_GP_FAULT            13
#define EXCEPTION_PAGE_FAULT          14
#define EXCEPTION_FPU_ERROR           16
#define EXCEPTION_ALIGN_CHECK         17
#define EXCEPTION_MACHINE_CHECK       18
#define EXCEPTION_SIMD_ERROR          19

/* --- Hardware Interrupts (IRQs) --- */
#define IRQ_BASE_OFFSET               32

#define IRQ0_TIMER                    0
#define IRQ1_KEYBOARD                 1
#define IRQ2_CASCADE                  2
#define IRQ3_COM2                     3
#define IRQ4_COM1                     4
#define IRQ5_LPT2                     5
#define IRQ6_FLOPPY                   6
#define IRQ7_LPT1                     7
#define IRQ8_CMOS                     8
#define IRQ12_MOUSE                   12
#define IRQ14_ATA1                    14
#define IRQ15_ATA2                    15

/* --- Vector Mapping Macros --- */
#define IRQ_TO_VECTOR(irq)            (IRQ_BASE_OFFSET + (irq))

#define TIMER_INTERRUPT_VECTOR        IRQ_TO_VECTOR(IRQ0_TIMER)
#define KEYBOARD_INTERRUPT_VECTOR     IRQ_TO_VECTOR(IRQ1_KEYBOARD)

/* --- Public API --- */

/**
 * @brief Registers a C function to handle a specific interrupt vector
 * @return Return true if the handler wants the kernel to panic
 */
void register_interrupt_handler(uint8_t vector, bool (*handler)(void));

/**
 * @brief Configures the PIC and loads the IDT into the CPU
 */
void idt_initialize(void);

/**
 * @brief Forces a system halt with a message
 */
void kpanic(const char* str);

/* --- Inline CPU Helpers --- */

static inline void enable_interrupts(void) {
    __asm__ volatile("sti"); 
}

static inline void disable_interrupts(void) {
    __asm__ volatile("cli"); 
}

static inline void wait_for_interrupt(void) {
    __asm__ volatile("hlt");
}

#endif