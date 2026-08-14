#ifndef GDT_H
#define GDT_H

#include <stdint.h>

typedef struct __attribute__((packed)) {
  uint16_t limit_low;
  uint16_t base_low;
  uint8_t base_middle;
  uint8_t access;
  uint8_t granularity;
  uint8_t base_high;
} gdt_entry_t;

typedef struct __attribute__((packed)) {
  uint32_t prev_tss;
  uint32_t esp0;
  uint32_t ss0;
  uint32_t esp1;
  uint32_t ss1;
  uint32_t esp2;
  uint32_t ss2;
  uint32_t cr3;
  uint32_t eip;
  uint32_t eflags;
  uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
  uint32_t es, cs, ss, ds, fs, gs;
  uint32_t ldt;
  uint16_t trap;
  uint16_t iomap_base;
} tss_entry_t;

typedef struct __attribute__((packed)) {
  uint16_t limit;
  uint32_t base;
} gdt_ptr_t;

/**
 * @brief Initializes the Global Descriptor Table
 *
 */
void gdt_initialize(void);
void tss_set_kernel_stack(uint32_t stack);
#endif