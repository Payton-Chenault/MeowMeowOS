#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define PAGE_SIZE 4096

typedef struct {
  uint64_t base;
  uint64_t length;
  uint32_t type;
  uint32_t acpi;
} __attribute__((packed)) mmap_entry_t;

void pmm_initialize(uint64_t mem_size, uint32_t bitmap_addr);
void pmm_initialize_from_map(void);
void *pmm_alloc_block(void);
void *pmm_alloc_contiguous_blocks(uint32_t count);
void pmm_free_block(void *ptr);
void pmm_mark_used(uint32_t addr);
void pmm_mark_free(uint32_t addr);
void pmm_get_memory_info(uint32_t *total, uint32_t *used, uint32_t *free);

#endif