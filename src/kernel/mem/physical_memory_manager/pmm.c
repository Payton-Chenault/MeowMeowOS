#include "pmm.h"
#include "../../utils/logging/logger.h"
#include <stdbool.h>

#define MODULE "PMM"

static uint32_t *pmm_bitmap = 0;
static uint32_t pmm_max_blocks = 0;

static inline bool pmm_block_is_valid(uint32_t block) {
  return pmm_bitmap != NULL && block < pmm_max_blocks;
}

static inline bool pmm_addr_is_valid(uint32_t addr) {
  if (pmm_bitmap == NULL || (addr % PAGE_SIZE) != 0) {
    return false;
  }
  return (addr / PAGE_SIZE) < pmm_max_blocks;
}

static inline void bitmap_set(uint32_t bit) {
  if (!pmm_block_is_valid(bit)) {
    return;
  }
  pmm_bitmap[bit / 32] |= (1 << (bit % 32));
}

static inline void bitmap_unset(uint32_t bit) {
  if (!pmm_block_is_valid(bit)) {
    return;
  }
  pmm_bitmap[bit / 32] &= ~(1 << (bit % 32));
}

static inline bool bitmap_test(uint32_t bit) {
  return pmm_block_is_valid(bit) &&
         (pmm_bitmap[bit / 32] & (1 << (bit % 32)));
}

void pmm_initialize(uint64_t mem_size, uint32_t bitmap_addr) {
  if (mem_size < PAGE_SIZE || bitmap_addr == 0) {
    log_error(MODULE, "FAILED: Invalid memory layout (mem_size=%llu, bitmap=0x%x)",
             (unsigned long long)mem_size, bitmap_addr);
    return;
  }
  pmm_max_blocks = (uint32_t)(mem_size / PAGE_SIZE);
  pmm_bitmap = (uint32_t *)bitmap_addr;
  uint32_t bitmap_words = (pmm_max_blocks + 31) / 32;

  for (uint32_t i = 0; i < bitmap_words; i++) {
    pmm_bitmap[i] = 0xFFFFFFFF;
  }

  for (uint32_t addr = 0; addr < 0x300000; addr += PAGE_SIZE) {
    pmm_mark_used(addr);
  }

  uint32_t bitmap_end = bitmap_addr + (bitmap_words * sizeof(uint32_t));
  for (uint32_t addr = bitmap_addr; addr < bitmap_end; addr += PAGE_SIZE) {
    pmm_mark_used(addr);
  }
  log_info(MODULE, "Initialized. Managing %d KB of RAM",
           (uint32_t)(mem_size / 1024));
}

void pmm_initialize_from_map() {
  uint32_t entry_count = *(uint32_t *)0x9000;
  if (entry_count == 0xFFFFFFFF || entry_count == 0) {
    log_warning(MODULE, "FAILED: BIOS Memory Map Failed! Using 32MB Safe Mode");
    pmm_initialize(32 * 1024 * 1024, 0x200000);
    for (uint32_t addr = 0x300000; addr < 0x2000000; addr += PAGE_SIZE) {
      pmm_mark_free(addr);
    }
    return;
  }

  log_debug(MODULE, "FOUND: Detected %d Memory Map Entries", entry_count);
  mmap_entry_t *entries = (mmap_entry_t *)0x9004;

  uint64_t highest_usable_addr = 0;
  for (uint32_t i = 0; i < entry_count; i++) {
    uint32_t base_low = (uint32_t)entries[i].base;
    uint32_t len_low = (uint32_t)entries[i].length;
    log_debug(MODULE, "FOUND: Entry %d: Base=%d, Len=%d, Type=%d", i, base_low,
              len_low, entries[i].type);
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

void *pmm_alloc_block(void) {
  if (pmm_bitmap == NULL || pmm_max_blocks == 0) {
    log_error(MODULE, "PMM not initialized");
    return NULL;
  }
  for (uint32_t i = 0; i < pmm_max_blocks / 32; i++) {
    if (pmm_bitmap[i] != 0xFFFFFFFF) {
      for (int j = 0; j < 32; j++) {
        uint32_t bit = 1U << j;
        if (!(pmm_bitmap[i] & bit)) {
          uint32_t block_index = (i * 32) + j;
          if (!pmm_block_is_valid(block_index)) {
            continue;
          }
          bitmap_set(block_index);
          return (void *)(block_index * PAGE_SIZE);
        }
      }
    }
  }
  log_error(MODULE, "OUT OF MEMORY!");
  return NULL;
}

void *pmm_alloc_contiguous_blocks(uint32_t count) {
    if (pmm_bitmap == NULL || pmm_max_blocks == 0 || count == 0) return NULL;
    uint32_t consecutive = 0;
    uint32_t start_bit = 0;
    for (uint32_t i = 0; i < pmm_max_blocks; i++) {
        if (!bitmap_test(i)) {
            if (consecutive == 0) start_bit = i;
            consecutive++;
            if (consecutive == count) {
                for (uint32_t j = 0; j < count; j++) bitmap_set(start_bit + j);
                log_trace(MODULE, "Allocated %u contiguous blocks starting at %x", count, start_bit * PAGE_SIZE);
                return (void *)(start_bit * PAGE_SIZE);
            }
        } else {
            consecutive = 0;
        }
    }
    log_error(MODULE, "Failed to allocate %u contiguous blocks", count);
    return NULL;
}

void pmm_free_block(void *ptr) {
  if (ptr == NULL) {
    return;
  }
  uint32_t addr = (uint32_t)ptr;
  if (!pmm_addr_is_valid(addr)) {
    log_error(MODULE, "Invalid block address 0x%x", addr);
    return;
  }
  uint32_t block = addr / PAGE_SIZE;
  log_trace(MODULE, "Freeing Memory block At Block Index %d.", block);
  bitmap_unset(block);
}

void pmm_mark_free(uint32_t addr) {
  if (!pmm_addr_is_valid(addr)) {
    return;
  }
  bitmap_unset(addr / PAGE_SIZE);
}

void pmm_mark_used(uint32_t addr) {
  if (!pmm_addr_is_valid(addr)) {
    return;
  }
  bitmap_set(addr / PAGE_SIZE);
}

void pmm_get_memory_info(uint32_t *total, uint32_t *used, uint32_t *free) {
  if (pmm_bitmap == NULL || pmm_max_blocks == 0) {
    *total = 0;
    *used = 0;
    *free = 0;
    return;
  }

  uint32_t used_blocks = 0;
  for (uint32_t i = 0; i < pmm_max_blocks / 32; i++) {
    if (pmm_bitmap[i] == 0) {
      continue; 
    }
    
    if (pmm_bitmap[i] == 0xFFFFFFFF) {
      used_blocks += 32; 
      continue;
    }
    for (int j = 0; j < 32; j++) {
      if (pmm_bitmap[i] & (1U << j)) {
        used_blocks++;
      }
    }
  }

  *total = pmm_max_blocks * PAGE_SIZE;
  *used = used_blocks * PAGE_SIZE;
  *free = *total - *used;
}