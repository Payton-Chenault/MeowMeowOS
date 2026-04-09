#include "elf.h"
#include "../../fs/fat_16/fat16.h"
#include "../../mem/virtual_memory_manager/vmm.h"
#include "../../mem/physical_memory_manager/pmm.h"
#include "../../lib/string/string.h"
#include "../../utils/logging/logger.h"
#include "../../kernel_services/kernel_services.h"
#include "../../arch/x86/task/task.h"
#include "../../arch/x86/global_descriptor_table/gdt.h"
#include <stdint.h>

#define MODULE "ELF_LOADER"

typedef void (*elf_entry_t)(void);

static uint32_t next_user_entry = 0;

static void user_mode_setup(void) {
    uint32_t* kernel_stack = (uint32_t*)kmem_zalloc(4096);
    tss_set_kernel_stack((uint32_t)kernel_stack + 4096);

    uint32_t user_stack_page = 0x80000000;
    void* user_stack_phys = pmm_alloc_block();

    vmm_map_page(user_stack_phys, (void*)user_stack_page, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

    uint32_t user_stack_top = user_stack_page + 4096 - 16;

    enter_ring3(next_user_entry, user_stack_top);
}


uint32_t elf_load_and_spawn(const char *filename) {
    uint32_t file_size = fat16_get_file_size(filename);
    if (file_size == 0) {
        log_error(MODULE, "FATAL: File not found or empty: %s", filename);
        return false;
    }

    uint8_t* file_buffer = kmem_zalloc(file_size);
    if(fat16_read_file(filename, file_buffer) == 0) {
        log_error(MODULE, "FATAL: Failed to read file: &s", filename);
        kmem_free(file_buffer);
        return false;
    }

    elf32_ehdr_t* header = (elf32_ehdr_t*)file_buffer;

    uint32_t magic = *(uint32_t*)header->e_ident;
    if(magic != ELF_MAGIC) {
        log_error(MODULE, "FATAL: Invalid ELF magic number! Is this a real program? &s", filename);
        kmem_free(file_buffer);
        return false;
    }

    if(header->e_type != 2) {
        log_error(MODULE, "FATAL: This ELF is not an executable! %s", filename);
        kmem_free(file_buffer);
        return false;
    }

    log_info(MODULE, "OK: Valid ELF found: %s, Entry point: %x", filename, header->e_entry);

    elf32_phdr_t* phdrs = (elf32_phdr_t*)(file_buffer + header->e_phoff);

    for (int i = 0; i< header->e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            uint32_t vaddr = phdrs[i].p_vaddr;
            uint32_t memsz = phdrs[i].p_memsz;
            uint32_t filesz = phdrs[i].p_filesz;
            uint32_t offset = phdrs[i].p_offset;

            uint32_t start_page = vaddr & ~0xFFF;
            uint32_t end_page = (vaddr + memsz + 0xFFF) & ~0xFFF;

            for (uint32_t page = start_page; page < end_page; page += 4096) {
                void* phys = pmm_alloc_block();
                vmm_map_page(phys, (void*)page, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
            }

            memcpy((void*)vaddr, file_buffer + offset, filesz);

            if (memsz > filesz) {
                memset((void*)(vaddr + filesz), 0, memsz - filesz);
            }
        }
    }




    next_user_entry = header->e_entry;
    uint32_t pid = task_create(filename, user_mode_setup);

    kmem_free(file_buffer);
    return pid;
}