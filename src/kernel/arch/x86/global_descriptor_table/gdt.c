#include "gdt.h"
#include <stdint.h>

#include "../../../utils/logging/logger.h"

#define MODULE "GDT"
static struct gdt_entry gdt[3];
static struct gdt_ptr gp;
extern void gdt_flush(uint32_t gdt_ptr_addr);

void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;

    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access = access;

    log_debug(MODULE, "OK: Loaded GDT Table #%d", num);
}

void gdt_initialize(void) {
    gp.limit = (sizeof(struct gdt_entry) * 3) - 1;
    gp.base = (uint32_t)&gdt;

    // Null descriptor
    gdt_set_gate(0, 0, 0, 0, 0);

    // Code Segment
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    gdt_flush((uint32_t)&gp);

    log_info(MODULE, "Initialized");
}