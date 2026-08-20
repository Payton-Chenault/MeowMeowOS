#ifndef VMM_H
#define VMM_H

#include <stdbool.h>
#include <stdint.h>

#define PAGE_SIZE 4096

#define PAGE_PRESENT 0X1
#define PAGE_WRITE 0x2
#define PAGE_USER 0x4

#define USER_VIRT_MIN 0x00100000u
#define USER_VIRT_MAX 0xBFFFFFFFu
#define KERNEL_VIRT_START 0xC0000000u
#define USER_STACK_TOP 0xBFFFF000u
#define USER_STACK_GUARD_MIN 0xBFC00000u

void vmm_initialize(void);
void *vmm_get_directory(void);
void *vmm_create_directory(void);
void vmm_map_page_in_directory(uint32_t page_dir_phys, void *phys, void *virt,
                               uint32_t flags);
void vmm_unmap_page_in_directory(uint32_t page_dir_phys, void *virt);
void vmm_switch_directory(void *directory);
void vmm_map_page(void *phys, void *virt, uint32_t flags);
void vmm_dump_pde(uint32_t pd_index);
bool page_fault_handler(void);
bool vmm_handle_user_page_fault(uint32_t fault_addr, uint32_t error_code);
extern void enable_paging(uint32_t *directory_addr);

#endif