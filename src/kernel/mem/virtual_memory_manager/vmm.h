#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stdbool.h>

#include "../physical_memory_manager/pmm.h"
#include "../../utils/logging/logger.h"
#include "../../utils/interrupt_descriptor_table/idt.h"


#define PAGE_SIZE 4096

#define PAGE_PRESENT  0X1
#define PAGE_WRITE 0x2
#define PAGE_USER 0x4

void vmm_initialize(void);
bool page_fault_handler(void);
extern void enable_paging(uint32_t* directory_addr);

#endif