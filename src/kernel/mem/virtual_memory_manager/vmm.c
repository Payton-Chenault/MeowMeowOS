#include "vmm.h"
#include "../physical_memory_manager/pmm.h"
#include "../../utils/logging/logger.h"
#include "../../arch/x86/interrupt_descriptor_table/idt.h"
#include "../../lib/string/string.h"
#include "../../kernel_services/kernel_services.h"
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

void* vmm_create_directory() {
    uint32_t* new_dir = (uint32_t*)pmm_alloc_block();
    if (!new_dir) return NULL;

    memset(new_dir, 0, PAGE_SIZE);

    // Copy all kernel page-directory entries except the recursive one
    for (int i = 0; i < 1023; i++) {
        new_dir[i] = kernel_directory[i];
    }

    // Set recursive entry to this new directory
    new_dir[1023] = ((uint32_t)new_dir) | PAGE_PRESENT | PAGE_WRITE;

    log_debug(MODULE, "Created new directory at phys=0x%x", new_dir);
    return (void*)new_dir;
}

void vmm_switch_directory(void* directory) {
    uint32_t phys_addr = (uint32_t)directory;
    log_debug(MODULE, "Switching CR3 to 0x%x", phys_addr);
    __asm__ volatile("mov %0, %%cr3" : : "r"(phys_addr) : "memory");
}

/**
 * Map a page into a *specific* page directory, without switching CR3.
 * The page directory physical address is given explicitly and is identity-mapped.
 */
void vmm_map_page_in_directory(uint32_t page_dir_phys, void* phys, void* virt, uint32_t flags) {
    uint32_t pd_index = (uint32_t)virt >> 22;
    uint32_t pt_index = ((uint32_t)virt >> 12) & 0x3FF;

    // Use the provided page directory directly (identity-mapped)
    uint32_t* page_directory = (uint32_t*)page_dir_phys;

    log_debug(MODULE, "map_page_in_dir: dir_phys=0x%x, phys=%x virt=%x flags=%x pd_index=%d pt_index=%d",
              page_dir_phys, phys, virt, flags, pd_index, pt_index);

    if (!(page_directory[pd_index] & PAGE_PRESENT)) {
        uint32_t new_table_phys = (uint32_t)pmm_alloc_block();
        if (new_table_phys == 0) {
            log_error(MODULE, "pmm_alloc_block failed for page table");
            return;
        }

        page_directory[pd_index] = new_table_phys | flags | PAGE_PRESENT;
        log_debug(MODULE, "  Allocated new page table phys=0x%x, PDE[%d]=0x%x",
                  new_table_phys, pd_index, page_directory[pd_index]);

        // Zero the new page table using its physical address (identity-mapped)
        uint32_t* new_table = (uint32_t*)new_table_phys;
        memset(new_table, 0, PAGE_SIZE);
        log_debug(MODULE, "  Page table zeroed");
    } else {
        log_debug(MODULE, "  PDE[%d] already present: 0x%x", pd_index, page_directory[pd_index]);
        // Upgrade permissions if needed
        page_directory[pd_index] |= (flags & (PAGE_USER | PAGE_WRITE | PAGE_PRESENT));
    }

    // Get the physical address of the page table from the PDE
    uint32_t table_phys = page_directory[pd_index] & ~0xFFF;
    uint32_t* page_table = (uint32_t*)table_phys;

    // Set the PTE
    page_table[pt_index] = (uint32_t)phys | flags | PAGE_PRESENT;
    log_debug(MODULE, "  Mapped PTE[%d] = 0x%x", pt_index, page_table[pt_index]);

    // No TLB flush needed because we are not changing the current CR3,
    // and the new mapping will be used only after a future CR3 switch.
}

void vmm_map_page(void* phys, void* virt, uint32_t flags) {
    // Read the current page directory physical address from CR3
    uint32_t page_dir_phys;
    __asm__ volatile("mov %%cr3, %0" : "=r"(page_dir_phys));

    vmm_map_page_in_directory(page_dir_phys, phys, virt, flags);
}

void vmm_dump_pde(uint32_t pd_index) {
    uint32_t page_dir_phys;
    __asm__ volatile("mov %%cr3, %0" : "=r"(page_dir_phys));
    uint32_t* page_directory = (uint32_t*)page_dir_phys;
    log_debug(MODULE, "PDE[%d] = 0x%x (CR3=0x%x)", pd_index, page_directory[pd_index], page_dir_phys);
}

bool page_fault_handler() {
    uint32_t faulting_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r" (faulting_addr));

    log_error(MODULE, "PAGE FAULT! Address: 0x%x", faulting_addr);
    return true;
}