#include "vmm.h"
#include "../../arch/x86/interrupt_descriptor_table/idt.h"
#include "../../kernel_services/kernel_services.h"
#include "../../lib/string/string.h"
#include "../../utils/logging/logger.h"
#include "../physical_memory_manager/pmm.h"
#include <stdint.h>

#define MODULE "VMM"

uint32_t *kernel_directory = 0;

static inline bool vmm_is_page_aligned(uint32_t value) {
  return (value & (PAGE_SIZE - 1)) == 0;
}

static inline bool vmm_is_valid_directory(uint32_t page_dir_phys) {
  return page_dir_phys != 0 && (page_dir_phys & (PAGE_SIZE - 1)) == 0;
}

static inline bool vmm_is_valid_page_index(uint32_t index) {
  return index < 1024;
}

void vmm_initialize() {
  kernel_directory = (uint32_t *)pmm_alloc_block();
  if (kernel_directory == NULL) {
    log_error(MODULE, "failed to allocate kernel page directory");
    return;
  }

  memset(kernel_directory, 0, PAGE_SIZE);

  uint32_t *identity_table = (uint32_t *)pmm_alloc_block();
  uint32_t *identity_table2 = (uint32_t *)pmm_alloc_block();

  if (identity_table == NULL || identity_table2 == NULL) {
    log_error(MODULE, "failed to allocate identity page tables");
    return;
  }

  for (uint32_t i = 0; i < 1024; i++) {
    identity_table[i] = (i * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITE;
  }

  for (uint32_t i = 0; i < 1024; i++) {
    identity_table2[i] = (0x400000 + (i * PAGE_SIZE)) | PAGE_PRESENT | PAGE_WRITE;
  }

  kernel_directory[0] = ((uint32_t)identity_table) | PAGE_PRESENT | PAGE_WRITE;
  kernel_directory[1] = ((uint32_t)identity_table2) | PAGE_PRESENT | PAGE_WRITE;
  kernel_directory[1023] =
      (uint32_t)kernel_directory | PAGE_PRESENT | PAGE_WRITE;

  enable_paging(kernel_directory);
  register_interrupt_handler(EXCEPTION_PAGE_FAULT, page_fault_handler);
  log_info(MODULE, "Initialized");
}

void *vmm_get_directory() { return (void *)kernel_directory; }

void *vmm_create_directory() {
  uint32_t *new_dir = (uint32_t *)pmm_alloc_block();
  if (!new_dir)
    return NULL;

  memset(new_dir, 0, PAGE_SIZE);

  for (int i = 0; i < 1023; i++) {
    new_dir[i] = kernel_directory[i];
  }

  new_dir[1023] = ((uint32_t)new_dir) | PAGE_PRESENT | PAGE_WRITE;

  log_debug(MODULE, "Created new directory at phys=0x%x", new_dir);
  return (void *)new_dir;
}

void vmm_switch_directory(void *directory) {
  uint32_t phys_addr = (uint32_t)directory;
  if (!vmm_is_page_aligned(phys_addr)) {
    log_error(MODULE, "invalid CR3 value 0x%x", phys_addr);
    return;
  }

  log_debug(MODULE, "Switching CR3 to 0x%x", phys_addr);
  __asm__ volatile("mov %0, %%cr3" : : "r"(phys_addr) : "memory");
}

void vmm_map_page_in_directory(uint32_t page_dir_phys, void *phys, void *virt,
                               uint32_t flags) {
  if (!vmm_is_valid_directory(page_dir_phys)) {
    log_error(MODULE, "invalid page directory physical address: 0x%x",
              page_dir_phys);
    return;
  }

  if (phys == NULL || virt == NULL) {
    log_error(MODULE, "cannot map NULL physical or virtual address");
    return;
  }

  if (!vmm_is_page_aligned((uint32_t)phys) ||
      !vmm_is_page_aligned((uint32_t)virt)) {
    log_error(MODULE,
              "misaligned mapping request: phys=0x%x virt=0x%x flags=0x%x",
              (uint32_t)phys, (uint32_t)virt, flags);
    return;
  }

  uint32_t pd_index = (uint32_t)virt >> 22;
  uint32_t pt_index = ((uint32_t)virt >> 12) & 0x3FF;

  if (!vmm_is_valid_page_index(pd_index) || !vmm_is_valid_page_index(pt_index)) {
    log_error(MODULE,
              "page index out of range: pd=%u pt=%u virt=0x%x",
              pd_index, pt_index, (uint32_t)virt);
    return;
  }

  uint32_t *page_directory = (uint32_t *)page_dir_phys;

  if (!(page_directory[pd_index] & PAGE_PRESENT)) {
    uint32_t new_table_phys = (uint32_t)pmm_alloc_block();
    if (new_table_phys == 0) {
      log_error(MODULE, "pmm_alloc_block failed for page table");
      return;
    }

    page_directory[pd_index] = (new_table_phys & ~0xFFF) | (flags & 0xFFF) |
                               PAGE_PRESENT;

    uint32_t *new_table = (uint32_t *)new_table_phys;
    memset(new_table, 0, PAGE_SIZE);
  }

  uint32_t table_phys = page_directory[pd_index] & ~0xFFF;
  if (table_phys == 0) {
    log_error(MODULE, "page table base is NULL after PDE lookup");
    return;
  }

  uint32_t *page_table = (uint32_t *)table_phys;
  page_table[pt_index] = ((uint32_t)phys & ~0xFFF) | (flags & 0xFFF) |
                         PAGE_PRESENT;
}

void vmm_map_page(void *phys, void *virt, uint32_t flags) {
  uint32_t page_dir_phys;
  __asm__ volatile("mov %%cr3, %0" : "=r"(page_dir_phys));

  vmm_map_page_in_directory(page_dir_phys, phys, virt, flags);
}

void vmm_map_region(uint32_t phys_start, uint32_t virt_start, uint32_t size, uint32_t flags) {
  uint32_t aligned_phys = phys_start & ~(PAGE_SIZE - 1);
  uint32_t aligned_virt = virt_start & ~(PAGE_SIZE - 1);
  uint32_t aligned_size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

  for (uint32_t offset = 0; offset < aligned_size; offset += PAGE_SIZE) {
    vmm_map_page((void *)(aligned_phys + offset), (void *)(aligned_virt + offset), flags);
  }
}

void vmm_unmap_page_in_directory(uint32_t page_dir_phys, void *virt) {
  if (!vmm_is_valid_directory(page_dir_phys) || virt == NULL) {
    return;
  }

  uint32_t virt_addr = (uint32_t)virt;
  if (!vmm_is_page_aligned(virt_addr)) {
    return;
  }

  uint32_t pd_index = virt_addr >> 22;
  uint32_t pt_index = (virt_addr >> 12) & 0x3FF;
  if (!vmm_is_valid_page_index(pd_index) || !vmm_is_valid_page_index(pt_index)) {
    return;
  }

  uint32_t *page_directory = (uint32_t *)page_dir_phys;
  if (!(page_directory[pd_index] & PAGE_PRESENT)) {
    return;
  }

  uint32_t *page_table = (uint32_t *)(page_directory[pd_index] & ~0xFFF);
  uint32_t pte = page_table[pt_index];
  if (!(pte & PAGE_PRESENT)) {
    return;
  }

  void *phys = (void *)(pte & ~0xFFF);
  page_table[pt_index] = 0;
  pmm_free_block(phys);
}

bool vmm_handle_user_page_fault(uint32_t fault_addr, uint32_t error_code) {
  (void)error_code;

  if (fault_addr < USER_VIRT_MIN || fault_addr >= KERNEL_VIRT_START) {
    return false;
  }

  uint32_t page = fault_addr & ~0xFFF;
  if (page >= USER_STACK_GUARD_MIN && page < USER_STACK_TOP) {
    void *phys = pmm_alloc_block();
    if (phys == NULL) {
      return false;
    }

    uint32_t page_dir_phys;
    __asm__ volatile("mov %%cr3, %0" : "=r"(page_dir_phys));
    vmm_map_page_in_directory(page_dir_phys, phys, (void *)page,
                              PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    return true;
  }

  return false;
}

void vmm_dump_pde(uint32_t pd_index) {
  uint32_t page_dir_phys;
  __asm__ volatile("mov %%cr3, %0" : "=r"(page_dir_phys));
  uint32_t *page_directory = (uint32_t *)page_dir_phys;
  log_debug(MODULE, "PDE[%d] = 0x%x (CR3=0x%x)", pd_index,
            page_directory[pd_index], page_dir_phys);
}

bool page_fault_handler() {
  uint32_t faulting_addr;
  __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_addr));

  log_error(MODULE, "PAGE FAULT! Address: 0x%x", faulting_addr);
  return true;
}