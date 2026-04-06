#include "vmm.h"
#include <stdint.h>

#define MODULE "VMM"

static uint32_t* kernel_directory = 0;

void vmm_initialize() {
    kernel_directory = (uint32_t*)pmm_alloc_block();

    for (int i = 0; i < 1024; i++) {
        kernel_directory[i] = 0 | PAGE_WRITE;
    }

    uint32_t* identity_table = (uint32_t*)pmm_alloc_block();
    uint32_t* identity_table2 = (uint32_t*)pmm_alloc_block();

    for(uint32_t i = 0; i < 1024; i++){
        identity_table[i] = (i * 4096) | PAGE_PRESENT | PAGE_WRITE;
    }

    for(uint32_t i = 0; i < 1024; i++) {
        identity_table2[i] = (0x400000 + (i * 4096)) | PAGE_PRESENT | PAGE_WRITE;
    }

    kernel_directory[0] = ((uint32_t)identity_table) | PAGE_PRESENT | PAGE_WRITE;
    kernel_directory[1] = ((uint32_t)identity_table2) | PAGE_PRESENT | PAGE_WRITE;

    enable_paging(kernel_directory);
    register_interrupt_handler(EXCEPTION_PAGE_FAULT, page_fault_handler);
    log_info(MODULE, "Virtual Memeory Enabled (Identity Mapped 0-8MB)");
}

bool page_fault_handler() {
    uint32_t faulting_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r" (faulting_addr));

    log_error(MODULE, "PAGE FAULT! Address: 0x%x", faulting_addr);

    return true;
}