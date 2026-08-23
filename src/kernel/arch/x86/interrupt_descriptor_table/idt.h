#ifndef IDT_H
#define IDT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * IDT Gate Type Attributes
 * P (1b) | DPL (2b) | S (1b) | Type (4b)
 */
#define IDT_FLAG_PRESENT 0x80
#define IDT_FLAG_RING0 0x00
#define IDT_FLAG_RING3 0x60
#define IDT_FLAG_GATE_32INT 0x0E // 32-bit Interrupt Gate
#define IDT_FLAG_GATE_32TRP 0x0F // 32-bit Trap Gate

/* Standard Gate for Ring 0 Interrupts: 1 (P) 00 (DPL) 0 (S) 1110 (Type) = 0x8E */
#define IDT_ATTR_INT_GATE_R0                                                   \
  (IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_FLAG_GATE_32INT)

/**
 * @brief Stack layout frame passed from assembly wrappers to C handlers
 */
typedef struct {
  uint32_t es, ds;
  uint32_t edi, esi, ebp, esp_ignored, ebx, edx, ecx, eax;
  uint32_t error_code;
  uint32_t eip, cs, eflags;
  uint32_t user_esp, user_ss; // Only pushed by CPU on privilege level change
} __attribute__((packed)) cpu_registers_t;

/**
 * @brief Represents a single entry in the Interrupt Descriptor Table
 */
typedef struct {
  uint16_t base_low;   // Lower 16 bits of the ISR address
  uint16_t sel;        // Kernel segment selector (usually 0x08)
  uint8_t always_zero; // Reserved, must be 0
  uint8_t flags;       // Attributes (Type, Privilege Level, Present bit)
  uint16_t base_high;  // Upper 16 bits of the ISR address
} __attribute__((packed)) idt_entry_t;

typedef struct {
  uint16_t limit; // Size of the IDT table - 1
  uint32_t base;  // Starting address of the idt_entry_t array
} __attribute__((packed)) idt_ptr_t;

#define EXCEPTION_DIV_BY_ZERO 0
#define EXCEPTION_DEBUG 1
#define EXCEPTION_NMI 2
#define EXCEPTION_BREAKPOINT 3
#define EXCEPTION_OVERFLOW 4
#define EXCEPTION_BOUND_RANGE 5
#define EXCEPTION_INVALID_OPCODE 6
#define EXCEPTION_DEV_NOT_AVAIL 7
#define EXCEPTION_DOUBLE_FAULT 8
#define EXCEPTION_COPROC_SEG_OVERRUN 9
#define EXCEPTION_INVALID_TSS 10
#define EXCEPTION_SEG_NOT_PRESENT 11
#define EXCEPTION_STACK_FAULT 12
#define EXCEPTION_GP_FAULT 13
#define EXCEPTION_PAGE_FAULT 14
#define EXCEPTION_FPU_ERROR 16
#define EXCEPTION_ALIGN_CHECK 17
#define EXCEPTION_MACHINE_CHECK 18
#define EXCEPTION_SIMD_ERROR 19

#define IRQ_BASE_OFFSET 32

#define IRQ0_TIMER 0
#define IRQ1_KEYBOARD 1
#define IRQ2_CASCADE 2
#define IRQ3_COM2 3
#define IRQ4_COM1 4
#define IRQ5_LPT2 5
#define IRQ6_FLOPPY 6
#define IRQ7_LPT1 7
#define IRQ8_CMOS 8
#define IRQ12_MOUSE 12
#define IRQ14_ATA1 14
#define IRQ15_ATA2 15

#define IRQ_TO_VECTOR(irq) (IRQ_BASE_OFFSET + (irq))

#define TIMER_INTERRUPT_VECTOR IRQ_TO_VECTOR(IRQ0_TIMER)
#define KEYBOARD_INTERRUPT_VECTOR IRQ_TO_VECTOR(IRQ1_KEYBOARD)
#define SYSCALL_INTERUPT_VECTOR 0x80

void register_interrupt_handler(uint8_t vector, bool (*handler)(void));
void idt_initialize(void);
void kpanic(const char *str);

void division_error_handler(cpu_registers_t *regs);
void gp_fault_handler(cpu_registers_t *regs);
bool page_fault_handler_with_error(cpu_registers_t *regs);
void default_exception_handler(cpu_registers_t *regs);

void print_register_dump(cpu_registers_t *regs);
void print_stack_trace(uint32_t max_frames, uint32_t starting_ebp);

static inline void enable_interrupts(void) { __asm__ volatile("sti"); }
static inline void disable_interrupts(void) { __asm__ volatile("cli"); }
static inline void wait_for_interrupt(void) { __asm__ volatile("hlt"); }

#endif