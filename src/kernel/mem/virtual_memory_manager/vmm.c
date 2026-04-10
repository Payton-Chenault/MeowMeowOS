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

    uint32_t total_blocks = pmm_get_max_block();

    uint32_t total_page_tables = (total_blocks + 1023) / 1024;

    if (total_page_tables > 1024) {
        total_page_tables = 1024;
    }

    log_info(MODULE, "Dynammicaly mapping %d Page Tables for %d MB of RAM", total_page_tables, (total_blocks * 4) / 1024);

    uint32_t phys_addr = 0;

    for (uint32_t pd_index = 0; pd_index < total_page_tables; pd_index++) {
        uint32_t* page_table = (uint32_t*)pmm_alloc_block();
        memset(page_table, 0, PAGE_SIZE);

        for (uint32_t pt_index = 0; pt_index < 1024; pt_index++) {
            if (phys_addr < (total_blocks * PAGE_SIZE)) {
                page_table[pt_index] = phys_addr | PAGE_PRESENT | PAGE_WRITE;
                phys_addr += PAGE_SIZE;
            }
        }

        kernel_directory[pd_index] = ((uint32_t)page_table) | PAGE_PRESENT | PAGE_WRITE;
    }
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