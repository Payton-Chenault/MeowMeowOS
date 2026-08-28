#include "idt.h"
#include "../../../drivers/ports/IO.h"
#include "../../../mem/virtual_memory_manager/vmm.h"
#include "../../../utils/logging/logger.h"
#include "../task/task.h"

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

extern void irq9_isr_wrapper(void);
extern void irq10_isr_wrapper(void);
extern void irq11_isr_wrapper(void);
extern void irq12_isr_wrapper(void);

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
  log_trace(MODULE, "Set IDT gate for vector 0x%X", num);
}

static void pic_configure(uint8_t master_offset, uint8_t slave_offset) {
  log_debug(MODULE, "Configuring 8259 PIC controllers (Master: 0x%X, Slave: 0x%X)", master_offset, slave_offset);
  outb(PIC1_COMMAND, ICW1_INIT);
  outb(PIC2_COMMAND, ICW1_INIT);
  outb(PIC1_DATA, master_offset);
  outb(PIC2_DATA, slave_offset);
  outb(PIC1_DATA, 4);
  outb(PIC2_DATA, 2);
  outb(PIC1_DATA, ICW4_8086);
  outb(PIC2_DATA, ICW4_8086);
  
  // Unmask IRQ 0 (Timer), IRQ 1 (Keyboard), and IRQ 2 (Cascade for Slave PIC / PCI NICs) -> 0xF8
  outb(PIC1_DATA, 0xF8);
  outb(PIC2_DATA, 0xFF);
}

void pic_unmask(uint8_t irq) {
  uint16_t port;
  if (irq < 8) {
      port = PIC1_DATA;
  } else {
      port = PIC2_DATA;
      irq -= 8;
  }
  uint8_t value = inb(port) & ~(1 << irq);
  outb(port, value);
  log_debug(MODULE, "Unmasked PIC IRQ %u on port 0x%X", irq, port);
}

void register_interrupt_handler(uint8_t vector, bool (*handler)(void)) {
  handlers[vector] = handler;
  log_debug(MODULE, "Registered handler for vector: 0x%x", vector);
}

void idt_initialize(void) {
  log_info(MODULE, "Initializing IDT and PIC...");
  for (int i = 0; i < 256; i++) {
    handlers[i] = NULL;
  }

  idtp.limit = (sizeof(idt_entry_t) * 256) - 1;
  idtp.base = (uint32_t)&idt;

  for (int i = 0; i < 256; i++) {
    set_idt_gate(i, (uint32_t)default_isr_wrapper, KERNEL_CS,
                 IDT_GATE_32BIT_INT);
  }

  set_idt_gate(EXCEPTION_DIV_BY_ZERO, (uint32_t)division_error_isr_wrapper, KERNEL_CS, 0x8E);
  set_idt_gate(EXCEPTION_PAGE_FAULT, (uint32_t)page_fault_isr_wrapper, KERNEL_CS, 0x8E);
  set_idt_gate(TIMER_INTERRUPT_VECTOR, (uint32_t)timer_isr_wrapper, KERNEL_CS, 0x8E);
  set_idt_gate(KEYBOARD_INTERRUPT_VECTOR, (uint32_t)keyboard_isr_wrapper, KERNEL_CS, 0x8E);
  set_idt_gate(SYSCALL_INTERUPT_VECTOR, (uint32_t)syscall_isr_wrapper, KERNEL_CS, 0xEE);
  set_idt_gate(EXCEPTION_GP_FAULT, (uint32_t)gp_fault_isr_wrapper, KERNEL_CS, 0x8E);

  set_idt_gate(IRQ_TO_VECTOR(9), (uint32_t)irq9_isr_wrapper, KERNEL_CS, 0x8E);
  set_idt_gate(IRQ_TO_VECTOR(10), (uint32_t)irq10_isr_wrapper, KERNEL_CS, 0x8E);
  set_idt_gate(IRQ_TO_VECTOR(11), (uint32_t)irq11_isr_wrapper, KERNEL_CS, 0x8E);
  set_idt_gate(IRQ_TO_VECTOR(12), (uint32_t)irq12_isr_wrapper, KERNEL_CS, 0x8E);

  pic_configure(0x20, 0x28);
  __asm__ volatile("lidt %0" : : "m"(idtp));

  log_info(MODULE, "IDT and PIC Initialized successfully");
}

void print_register_dump(cpu_registers_t *regs) {
  log_error("CRASH", "--- Register Dump ---");
  log_error("CRASH", "EAX: 0x%x  EBX: 0x%x  ECX: 0x%x  EDX: 0x%x", regs->eax, regs->ebx, regs->ecx, regs->edx);
  log_error("CRASH", "ESI: 0x%x  EDI: 0x%x  EBP: 0x%x  ESP: 0x%x", regs->esi, regs->edi, regs->ebp, regs->esp_ignored);
  log_error("CRASH", "EIP: 0x%x  CS:  0x%x  EFLAGS: 0x%x", regs->eip, regs->cs, regs->eflags);
  log_error("CRASH", "DS:  0x%x  ES:  0x%x  ERR_CODE: 0x%x", regs->ds, regs->es, regs->error_code);
  if (regs->cs != KERNEL_CS) {
    log_error("CRASH", "USER ESP: 0x%x  USER SS: 0x%x", regs->user_esp, regs->user_ss);
  }
}

void print_stack_trace(uint32_t max_frames, uint32_t starting_ebp) {
  log_error("CRASH", "--- Stack Trace ---");
  uint32_t *ebp = (uint32_t *)starting_ebp;
  for (uint32_t frame = 0; frame < max_frames; ++frame) {
    if ((uint32_t)ebp < 0x1000 || (uint32_t)ebp % 4 != 0) {
      break; 
    }
    
    uint32_t eip = ebp[1];
    if (eip == 0) break;
    
    log_error("CRASH", "  [Frame %d] EIP: 0x%x", frame, eip);
    ebp = (uint32_t *)ebp[0];
  }
}

// ==========================================
// Exception Handlers
// ==========================================

void division_error_handler(cpu_registers_t *regs) {
  log_error("CPU", "Division by Zero Exception!");
  print_register_dump(regs);
  print_stack_trace(10, regs->ebp);

  task_t *cur = task_get_current();
  if ((regs->cs & 0x3) != 0 && cur != NULL) {
    log_error("CPU", "Sending SIGFPE to faulting user task %s (PID: %u)", cur->name, cur->pid);
    task_send_signal(cur->pid, SIGFPE);
    task_check_signals();
  } else {
    kpanic("Division Error");
  }
}

void gp_fault_handler(cpu_registers_t *regs) {
  log_error("CPU", "General Protection Fault!");
  print_register_dump(regs);
  print_stack_trace(10, regs->ebp);
  
  task_t *cur = task_get_current();
  if ((regs->cs & 0x3) != 0 && cur != NULL) {
    log_error("CPU", "Sending SIGSEGV to faulting user task %s (PID: %u)", cur->name, cur->pid);
    task_send_signal(cur->pid, SIGSEGV);
    task_check_signals();
  } else {
    kpanic("General Protection Fault");
  }
}

void default_exception_handler(cpu_registers_t *regs) {
  log_error("CPU", "Unhandled Exception!");
  print_register_dump(regs);
  print_stack_trace(10, regs->ebp);
  
  task_t *cur = task_get_current();
  if ((regs->cs & 0x3) != 0 && cur != NULL) {
    log_error("CPU", "Sending SIGILL to faulting user task %s (PID: %u)", cur->name, cur->pid);
    task_send_signal(cur->pid, SIGILL);
    task_check_signals();
  } else {
    kpanic("Unhandled Exception");
  }
}

bool page_fault_handler_with_error(cpu_registers_t *regs) {
  uint32_t faulting_addr;
  __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_addr));

  bool present = regs->error_code & 0x1;
  bool rw = regs->error_code & 0x2;
  bool user = regs->error_code & 0x4;
  bool reserved = regs->error_code & 0x8;
  bool id = regs->error_code & 0x10;

  log_error(MODULE, "PAGE FAULT at 0x%x", faulting_addr);
  log_error(MODULE, "Reason: %s %s page %s",
            present ? "Protection violation on" : "Non-present",
            user ? "user" : "supervisor",
            rw ? "write" : "read");
  if (reserved) log_error(MODULE, "Reserved bit overwritten!");
  if (id) log_error(MODULE, "Occurred during instruction fetch");

  if (faulting_addr >= USER_VIRT_MIN && faulting_addr < KERNEL_VIRT_START) {
    if (vmm_handle_user_page_fault(faulting_addr, regs->error_code)) {
      log_warning(MODULE, "Recovered user page fault at 0x%x", faulting_addr);
      return false;
    }
    log_error(MODULE, "User page fault unhandled; terminating task");
  }

  uint32_t page_dir_phys;
  __asm__ volatile("mov %%cr3, %0" : "=r"(page_dir_phys));
  uint32_t *pd = (uint32_t *)page_dir_phys;
  uint32_t pd_index = faulting_addr >> 22;
  uint32_t pt_index = (faulting_addr >> 12) & 0x3FF;
  
  if (pd[pd_index] & PAGE_PRESENT) {
    uint32_t table_phys = pd[pd_index] & ~0xFFF;
    uint32_t *pt = (uint32_t *)table_phys;
    uint32_t pte = pt[pt_index];
    log_error(MODULE, "PDE=0x%x, PTE=0x%x", pd[pd_index], pte);
  } else {
    log_error(MODULE, "PDE not present: 0x%x", pd[pd_index]);
  }

  print_register_dump(regs);
  print_stack_trace(10, regs->ebp);

  task_t *cur = task_get_current();
  if ((regs->cs & 0x3) != 0 && cur != NULL) {
    task_send_signal(cur->pid, SIGSEGV);
    task_check_signals();
    return false;
  }

  return true;
}

void interrupt_dispatcher(uint32_t vector) {
  log_trace(MODULE, "Interrupt dispatcher invoked for vector 0x%X", vector);
  if (handlers[vector] != NULL) {
    bool request_panic = handlers[vector]();
    if (request_panic) {
      log_error(MODULE, "Critical failure in handler 0x%x", vector);
      kpanic("Interrupt handler requested immediate system halt");
    }
  } else if (vector < 32) {
    log_error(MODULE, "Unhandled Processor Exception: 0x%x", vector);
    kpanic("Processor Exception (Kernel Halt)");
  } else {
    log_warning(MODULE, "No handler for IRQ vector: 0x%x", vector);
  }

  if (vector >= 40) {
    outb(PIC2_COMMAND, PIC_EOI);
  }
  outb(PIC1_COMMAND, PIC_EOI);
}

void debug_syscall_frame(uint32_t cs, uint32_t ss, uint32_t eip, uint32_t esp) {
  log_debug("SYSCALL", "Return frame: CS=0x%x SS=0x%x EIP=0x%x ESP=0x%x", cs,
            ss, eip, esp);
}