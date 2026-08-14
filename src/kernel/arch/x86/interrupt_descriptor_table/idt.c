#include "idt.h"
#include "../../../drivers/ports/IO.h"
#include "../../../mem/virtual_memory_manager/vmm.h"
#include "../../../utils/logging/logger.h"

#define MODULE "IDT"

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20

#define ICW1_INIT 0x11
#define ICW4_8086 0x01

#define IDT_GATE_32BIT_INT 0x8E
#define KERNEL_CS 0x08

extern void keyboard_isr_wrapper(void);
extern void timer_isr_wrapper(void);
extern void page_fault_isr_wrapper(void);
extern void default_isr_wrapper(void);
extern void syscall_isr_wrapper(void);
extern void division_error_isr_wrapper(void);
extern void gp_fault_isr_wrapper(void);

static idt_entry_t idt[256];
static idt_ptr_t idtp;

static bool (*handlers[256])(void);

static void set_idt_gate(uint8_t num, uint32_t base, uint16_t selector,
                         uint8_t flags) {
  idt[num].base_low = (base & 0xFFFF);
  idt[num].base_high = (base >> 16) & 0xFFFF;
  idt[num].sel = selector;
  idt[num].always_zero = 0;
  idt[num].flags = flags;
}

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

void division_error_handler(uint32_t eip) {
  log_error("CPU", "Division Error at EIP: 0x%x", eip);

  // Try to read the bytes at EIP for diagnosis
  uint32_t page_dir_phys;
  __asm__ volatile("mov %%cr3, %0" : "=r"(page_dir_phys));
  uint32_t *pd = (uint32_t *)page_dir_phys;

  uint32_t pd_index = eip >> 22;
  uint32_t pt_index = (eip >> 12) & 0x3FF;

  if (pd[pd_index] & PAGE_PRESENT) {
    uint32_t table_phys = pd[pd_index] & ~0xFFF;
    uint32_t *pt = (uint32_t *)table_phys;
    uint32_t pte = pt[pt_index];
    if (pte & PAGE_PRESENT) {
      uint32_t phys = (pte & ~0xFFF) + (eip & 0xFFF);
      uint8_t *bytes = (uint8_t *)phys;
      log_error("CPU", "Bytes at EIP: %x %x %x %x %x %x %x %x", bytes[0],
                bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6],
                bytes[7]);
    }
  }

  kpanic("Division Error");
}

void idt_initialize(void) {
  for (int i = 0; i < 256; i++) {
    handlers[i] = NULL;
  }

  idtp.limit = (sizeof(idt_entry_t) * 256) - 1;
  idtp.base = (uint32_t)&idt;

  // Fill all with default handler
  for (int i = 0; i < 256; i++) {
    set_idt_gate(i, (uint32_t)default_isr_wrapper, KERNEL_CS,
                 IDT_GATE_32BIT_INT);
  }

  // Override specific vectors
  set_idt_gate(EXCEPTION_DIV_BY_ZERO, (uint32_t)division_error_isr_wrapper,
               KERNEL_CS, 0x8E); // DIV/0

  uint32_t base = (idt[0].base_high << 16) | idt[0].base_low;
  log_debug(MODULE, "IDT[0] base = 0x%x (should match wrapper)", base);
  log_debug(MODULE, "division_error_isr_wrapper = 0x%x",
            (uint32_t)division_error_isr_wrapper);

  set_idt_gate(EXCEPTION_PAGE_FAULT, (uint32_t)page_fault_isr_wrapper,
               KERNEL_CS, 0x8E);
  set_idt_gate(TIMER_INTERRUPT_VECTOR, (uint32_t)timer_isr_wrapper, KERNEL_CS,
               0x8E);
  set_idt_gate(KEYBOARD_INTERRUPT_VECTOR, (uint32_t)keyboard_isr_wrapper,
               KERNEL_CS, 0x8E);
  set_idt_gate(SYSCALL_INTERUPT_VECTOR, (uint32_t)syscall_isr_wrapper,
               KERNEL_CS, 0xEE);
  set_idt_gate(EXCEPTION_GP_FAULT, (uint32_t)gp_fault_isr_wrapper, KERNEL_CS,
               0x8E);

  pic_configure(0x20, 0x28);

  __asm__ volatile("lidt %0" : : "m"(idtp));

  log_info(MODULE, "IDT and PIC Initialized");
}

bool page_fault_handler_with_error(uint32_t error_code, uint32_t eip) {
  uint32_t faulting_addr;
  __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_addr));

  log_error(MODULE, "PAGE FAULT at %x, error code: %x, EIP: %x", faulting_addr,
            error_code, eip);

  uint32_t page_dir_phys;
  __asm__ volatile("mov %%cr3, %0" : "=r"(page_dir_phys));
  uint32_t *pd = (uint32_t *)page_dir_phys;

  uint32_t pd_index = faulting_addr >> 22;
  uint32_t pt_index = (faulting_addr >> 12) & 0x3FF;

  if (pd[pd_index] & PAGE_PRESENT) {
    uint32_t table_phys = pd[pd_index] & ~0xFFF;
    uint32_t *pt = (uint32_t *)table_phys;
    uint32_t pte = pt[pt_index];
    log_error(MODULE, "PDE=%x, PTE=%x", pd[pd_index], pte);
  } else {
    log_error(MODULE, "PDE not present: %x", pd[pd_index]);
  }

  return true;
}

void interrupt_dispatcher(uint32_t vector) {
  if (handlers[vector] != NULL) {
    bool request_panic = handlers[vector]();
    if (request_panic) {
      log_error(MODULE, "Critical failure in handler %x", vector);
      kpanic("Interrupt handler requested immediate system halt");
    }
  } else if (vector < 32) {
    log_error(MODULE, "Unhandled Processor Exception: %x", vector);
    kpanic("Processor Exception (Kernel Halt)");
  } else {
    log_warning(MODULE, "No handler for IRQ vector: %x", vector);
  }

  if (vector >= 40) {
    outb(PIC2_COMMAND, PIC_EOI);
  }
  outb(PIC1_COMMAND, PIC_EOI);
}

void gp_fault_handler(uint32_t error_code, uint32_t eip) {
  log_error("CPU", "GP FAULT at EIP: 0x%x, error code: 0x%x", eip, error_code);

  // Try to read bytes at EIP for diagnosis
  uint32_t page_dir_phys;
  __asm__ volatile("mov %%cr3, %0" : "=r"(page_dir_phys));
  uint32_t *pd = (uint32_t *)page_dir_phys;

  uint32_t pd_index = eip >> 22;
  uint32_t pt_index = (eip >> 12) & 0x3FF;
  if (pd[pd_index] & PAGE_PRESENT) {
    uint32_t table_phys = pd[pd_index] & ~0xFFF;
    uint32_t *pt = (uint32_t *)table_phys;
    uint32_t pte = pt[pt_index];
    if (pte & PAGE_PRESENT) {
      uint32_t phys = (pte & ~0xFFF) + (eip & 0xFFF);
      uint8_t *bytes = (uint8_t *)phys;
      log_error("CPU", "Bytes at EIP: %x %x %x %x %x %x %x %x", bytes[0],
                bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6],
                bytes[7]);
    }
  }

  kpanic("General Protection Fault");
}

void default_exception_handler(uint32_t eip) {
  log_error(MODULE, "Unhandled exception at EIP: 0x%x", eip);

  // Read bytes at EIP
  uint32_t page_dir_phys;
  __asm__ volatile("mov %%cr3, %0" : "=r"(page_dir_phys));
  uint32_t *pd = (uint32_t *)page_dir_phys;

  uint32_t pd_index = eip >> 22;
  uint32_t pt_index = (eip >> 12) & 0x3FF;
  if (pd[pd_index] & PAGE_PRESENT) {
    uint32_t table_phys = pd[pd_index] & ~0xFFF;
    uint32_t *pt = (uint32_t *)table_phys;
    uint32_t pte = pt[pt_index];
    if (pte & PAGE_PRESENT) {
      uint32_t phys = (pte & ~0xFFF) + (eip & 0xFFF);
      uint8_t *bytes = (uint8_t *)phys;
      log_error(MODULE, "Bytes at EIP: %x %x %x %x %x %x %x %x", bytes[0],
                bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6],
                bytes[7]);
    }
  }

  kpanic("Unhandled Exception");
}

void debug_syscall_frame(uint32_t cs, uint32_t ss, uint32_t eip, uint32_t esp) {
  log_debug("SYSCALL", "Return frame: CS=0x%x SS=0x%x EIP=0x%x ESP=0x%x", cs,
            ss, eip, esp);
}