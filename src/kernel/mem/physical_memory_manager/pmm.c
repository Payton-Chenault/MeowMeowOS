#include "pmm.h"

#include "../../utils/logging/logger.h"
#include <stdbool.h>

#define MODULE "PMM"

static uint32_t* pmm_bitmap = 0;
static uint32_t pmm_max_blocks = 0;

/**
 * @brief Sets a specific bit in the bitmap to 1
 * 
 * @param bit The index of the 4KB block (Address / 4096)
 */
static inline void bitmap_set(uint32_t bit) {
    pmm_bitmap[bit / 32] |= (1 << (bit % 32));
}

/**
 * @brief Unset a specific bit
 * 
 * @param bit the bit in the bitmap to unset
 */
static inline void bitmap_unset(uint32_t bit) {
    pmm_bitmap[bit / 32] &= ~(1 << (bit % 32));
}

/**
 * @brief Tests to see if a bit is set in the bitmap
 * 
 * @param bit the bit to test
 * @return true -> bitmap bit is present
 * @return false -> bitmap bit is empty
 */
static inline bool bitmap_test(uint32_t bit) {
    return pmm_bitmap[bit / 32] & (1 << (bit % 32));
}

void pmm_initialize(uint64_t mem_size, uint32_t bitmap_addr) {
    pmm_max_blocks = (uint32_t)(mem_size / PAGE_SIZE);
    pmm_bitmap = (uint32_t*)bitmap_addr;

    // Needed to ensure all memory is marked as used before attempting to hand it out (So kernel and other stuff doenst get obliterated in mem)
    for (uint32_t i = 0; i < pmm_max_blocks / 32; i++) {
        pmm_bitmap[i] = 0xFFFFFFFF; 
    }
    
    log_info(MODULE, "Initialized. Managing %d KB of RAM", (uint32_t)(mem_size / 1024));
}

void pmm_initialize_from_map() {
    uint32_t entry_count = *(uint32_t*)0x9000;

    if (entry_count == 0xFFFFFFFF || entry_count == 0) {
        log_warning(MODULE, "FAILED: BIOS Memory Map Failed! Using 32MB Safe Mode");

        pmm_initialize(32 * 1024 * 1024, 0x200000);

        for (uint32_t addr = 0x300000; addr < 0x2000000; addr += PAGE_SIZE) {
            pmm_mark_free(addr);
        }
        return;
    }

    log_debug(MODULE, "FOUND: Detected %d Memory Map Entries", entry_count);

    mmap_entry_t* entries = (mmap_entry_t*)0x9004;

    uint64_t highest_usable_addr = 0;

    for (uint32_t i = 0; i < entry_count; i++) {

        uint32_t base_low = (uint32_t)entries[i].base;
        uint32_t len_low = (uint32_t)entries[i].length;
        log_debug(MODULE, "FOUND: Entry %d: Base=%d, Len=%d, Type=%d", i, base_low, len_low, entries[i].type);

        if (entries[i].type == 1) {
            uint64_t end_of_region = entries[i].base + entries[i].length;
            if (end_of_region > highest_usable_addr) {
                highest_usable_addr = end_of_region;
            }
        }
    }

    pmm_initialize(highest_usable_addr, 0x200000);


    for (uint32_t i = 0; i < entry_count; i++) {
        if (entries[i].type == 1) {
            uint64_t addr = entries[i].base;
            uint64_t end = entries[i].base + entries[i].length;

            while (addr < end) {
                if (addr >= 0x300000) {
                    pmm_mark_free((uint32_t)addr);
                }

                addr += PAGE_SIZE;
            }
        }
    }

    log_debug(MODULE, "OK: Finished Parsing BIOS Memory Map");
}

void* pmm_alloc_block(void) {
    for (uint32_t i = 0; i < pmm_max_blocks / 32; i++) {
        if (pmm_bitmap[i] != 0xFFFFFFFF) {
            for (int j = 0; j < 32; j++) {
                int bit = 1 << j;
                if(!(pmm_bitmap[i] & bit)) {
                    uint32_t block_index = (i * 32) + j;
                    bitmap_set(block_index);
                    return (void*)(block_index * PAGE_SIZE);
                }
            }
        }
    }
    log_error(MODULE, "OUT OF MEMORY!");
    return NULL;
}

void pmm_free_block(void *ptr) {
    uint32_t addr = (uint32_t)ptr;
    uint32_t block = addr / PAGE_SIZE;
    log_warning(MODULE, "Freeing Memory block At Block Index %d.", block);
    bitmap_unset(block);
}

void pmm_mark_free(uint32_t addr) {
    bitmap_unset(addr / PAGE_SIZE);
}

void pmm_mark_used(uint32_t addr) {
    bitmap_set(addr / PAGE_SIZE);
}

uint32_t pmm_get_max_block(void) {
    return pmm_max_blocks;
}

