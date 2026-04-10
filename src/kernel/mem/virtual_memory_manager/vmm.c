#include "vmm.h"

#include "../physical_memory_manager/pmm.h"
#include "../../utils/logging/logger.h"
#include "../../arch/x86/interrupt_descriptor_table/idt.h"
#include "../../lib/string/string.h"
#include <stdint.h>

#define MODULE "VMM"

uint32_t* kernel_directory = 0;

void vmm_initialize() {
    kernel_directory = (uint32_t*)pmm_alloc_block();

    memset(kernel_directory, 0, PAGE_SIZE);

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
    kernel_directory[1023] = (uint32_t)kernel_directory | PAGE_PRESENT | PAGE_WRITE;

    enable_paging(kernel_directory);
    register_interrupt_handler(EXCEPTION_PAGE_FAULT, page_fault_handler);
    log_info(MODULE, "Initialized");
}

void* vmm_get_directory() {
    return (void*)kernel_directory;
}

void vmm_map_page(void* phys, void* virt, uint32_t flags) {
    uint32_t pd_index = (uint32_t)virt >> 22;
    uint32_t pt_index = ((uint32_t)virt >> 12) & 0x3FF;

    uint32_t* page_directory = (uint32_t*)0xFFFFF000;
    uint32_t* page_table = (uint32_t*)(0xFFC00000 + (pd_index * 4096));

    if (!(page_directory[pd_index] & PAGE_PRESENT)) {
        
        uint32_t new_table_phys = (uint32_t)pmm_alloc_block();
        
        page_directory[pd_index] = new_table_phys | flags | PAGE_PRESENT;
        
        __asm__ volatile ("invlpg (%0)" :: "r"(page_table) : "memory");
        
        memset(page_table, 0, PAGE_SIZE);
        
    }

    page_table[pt_index] = (uint32_t)phys | flags | PAGE_PRESENT; 

    __asm__ volatile ("invlpg (%0)" :: "r"(virt) : "memory");
}

bool page_fault_handler() {
    uint32_t faulting_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r" (faulting_addr));

    log_error(MODULE, "PAGE FAULT! Address: 0x%x", faulting_addr);

    return true;
}