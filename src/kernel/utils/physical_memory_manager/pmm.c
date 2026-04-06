#include "pmm.h"
#include "../../utils/logging/logger.h"
#include <stdint.h>

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

void pmm_init(uint32_t mem_size, uint32_t bitmap_addr) {
    pmm_max_blocks = mem_size / PAGE_SIZE;
    pmm_bitmap = (uint32_t*)bitmap_addr;

    // Needed to ensure all memory is marked as used before attempting to hand it out (So kernel and other stuff doenst get obliterated in mem)
    for (uint32_t i = 0; i < pmm_max_blocks / 32; i++) {
        pmm_bitmap[i] = 0xFFFFFFFF; 
    }

    log_info(MODULE, "PMM Initialized. Managing %d KB of RAM", mem_size / 1024);
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
    bitmap_unset(block);
}

void pmm_mark_free(uint32_t addr) {
    bitmap_unset(addr / PAGE_SIZE);
}

void pmm_mark_used(uint32_t addr) {
    bitmap_set(addr / PAGE_SIZE);
}

