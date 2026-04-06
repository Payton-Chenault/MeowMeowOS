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

    for(uint32_t i = 0; i < 1024; i++){
        identity_table[i] = (i * 4096) | PAGE_PRESENT | PAGE_WRITE;
    }

    kernel_directory[0] = ((uint32_t)identity_table) | PAGE_PRESENT | PAGE_WRITE;

    enable_paging(kernel_directory);
    log_info(MODULE, "Virtual Memeory Enabled (Identity Mapped 0-4MB)");
}