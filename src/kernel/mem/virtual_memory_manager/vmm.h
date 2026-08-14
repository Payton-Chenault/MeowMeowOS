#ifndef VMM_H
#define VMM_H

#include <stdbool.h>
#include <stdint.h>

#define PAGE_SIZE 4096

#define PAGE_PRESENT 0X1
#define PAGE_WRITE 0x2
#define PAGE_USER 0x4

void vmm_initialize(void);
void *vmm_get_directory(void);
void *vmm_create_directory(void);
void vmm_map_page_in_directory(uint32_t page_dir_phys, void *phys, void *virt,
                               uint32_t flags);
void vmm_switch_directory(void *directory);
void vmm_map_page(void *phys, void *virt, uint32_t flags);
void vmm_dump_pde(uint32_t pd_index);
bool page_fault_handler(void);
extern void enable_paging(uint32_t *directory_addr);

#endif