#include "gdt.h"
#include <stdint.h>

#include "../../../lib/string/string.h"
#include "../../../utils/logging/logger.h"

#define MODULE "GDT"
static gdt_entry_t gdt[7];
static gdt_ptr_t gp;
tss_entry_t tss_entry;

extern void gdt_flush(uint32_t);
extern void tss_flush(void);

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access,
                         uint8_t gran) {
  gdt[num].base_low = (base & 0xFFFF);
  gdt[num].base_middle = (base >> 16) & 0xFF;
  gdt[num].base_high = (base >> 24) & 0xFF;

  gdt[num].limit_low = (limit & 0xFFFF);
  gdt[num].granularity = (limit >> 16) & 0x0F;

  gdt[num].granularity |= gran & 0xF0;
  gdt[num].access = access;

  log_debug(MODULE, "OK: Loaded GDT Table #%d", num);
}

static void write_tss(int32_t num, uint16_t ss0, uint32_t esp0) {
  uint32_t base = (uint32_t)&tss_entry;
  uint32_t limit = sizeof(tss_entry_t) - 1;

  gdt_set_gate(num, base, limit, 0xE9, 0x00);

  memset(&tss_entry, 0, sizeof(tss_entry_t));

  tss_entry.ss0 = ss0;
  tss_entry.esp0 = esp0;

  tss_entry.iomap_base = sizeof(tss_entry_t);
}

void gdt_initialize(void) {
  gp.limit = (sizeof(gdt_entry_t) * 7) - 1;
  gp.base = (uint32_t)&gdt;

  gdt_set_gate(0, 0, 0, 0, 0);

  gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

  gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

  gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

  gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

  write_tss(5, 0x10, 0x0);

  gdt_flush((uint32_t)&gp);
  tss_flush();

  log_info(MODULE, "Initialized");
}

void tss_set_kernel_stack(uint32_t stack) { tss_entry.esp0 = stack; }