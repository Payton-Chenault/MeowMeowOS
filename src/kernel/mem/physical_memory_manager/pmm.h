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

/**
 * @brief Initialized the Physical Memory Manager
 *
 * @param mem_size Total RAM in Bytes
 * @param bitmap_addr The physical adress where the bitmap will stay
 *
 * @note Memory must be specificly freed utilizing this mode of initialization
 */
void pmm_initialize(uint64_t mem_size, uint32_t bitmap_addr);

/**
 * @brief Initializes the Physical Memory Manager using the BIOS's Memory Map
 *
 * @note Unlike pmm_initialize, this automatically frees up all memory available
 * from 3MB -> nMB
 */
void pmm_initialize_from_map(void);

/**
 * @brief Allocates one block of 4KB memory from the physical ram
 *
 * @return a pointer to the start of the 4KB Block or NULL if none is left
 */
void *pmm_alloc_block(void);

/**
 * @brief Frees a allocated 4KB Block
 *
 * @param ptr The physical address of the block to return to the pool
 */
void pmm_free_block(void *ptr);

/**
 * @brief Manually marks a specific region of memory as "In Use"
 *
 * @note Used during boot to protect the Kernel, GDT, and IDT from being
 * overwritten
 *
 * @param addr
 */
void pmm_mark_used(uint32_t addr);

/**
 * @brief Manually marks a region as "Free"
 *
 * @param addr
 */
void pmm_mark_free(uint32_t addr);

/**
 * @brief Scans the bitmap to report RAM usage
 */
void pmm_get_memory_info(uint32_t *total, uint32_t *used, uint32_t *free);

#endif