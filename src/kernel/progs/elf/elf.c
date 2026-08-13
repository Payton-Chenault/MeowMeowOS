#include "elf.h"
#include "../../fs/fat_16/fat16_vfs.h"
#include "../../mem/virtual_memory_manager/vmm.h"
#include "../../mem/physical_memory_manager/pmm.h"
#include "../../lib/string/string.h"
#include "../../utils/logging/logger.h"
#include "../../kernel_services/kernel_services.h"
#include "../../arch/x86/task/task.h"
#include "../../arch/x86/global_descriptor_table/gdt.h"
#include "../../drivers/keyboard/keyboard.h"
#include <stdint.h>

#define MODULE "ELF_LOADER"

uint32_t elf_load_and_spawn(const char *filename) {
    log_debug(MODULE, "ELF loader: trying to open %s", filename);

    vfs_node_t* node = vfs_find(filename);
    if (node == NULL) {
        log_debug(MODULE, "vfs_find failed, trying fat16_vfs_open");
        node = fat16_vfs_open(filename);
    }

    if (node == NULL) {
        log_error(MODULE, "FATAL: File not found: %s", filename);
        return 0;
    }

    log_debug(MODULE, "File found, length = %u", node->length);

    if (node->length < sizeof(elf32_ehdr_t)) {
        log_error(MODULE, "FATAL: File too small to be an ELF: %s", filename);
        if (node->type == VFS_FILE) kmem_free(node);
        return 0;
    }

    elf32_ehdr_t header;
    if (vfs_read(node, 0, sizeof(elf32_ehdr_t), (uint8_t*)&header) == 0) {
        log_error(MODULE, "FATAL: Failed to read ELF header");
        if (node->type == VFS_FILE) kmem_free(node);
        return 0;
    }

    uint32_t magic = *(uint32_t*)header.e_ident;
    if (magic != ELF_MAGIC || header.e_type != 2) {
        log_error(MODULE, "FATAL: Invalid ELF executable: %s", filename);
        if (node->type == VFS_FILE) kmem_free(node);
        return 0;
    }

    log_info(MODULE, "OK: Valid ELF found: %s, Entry point: %x", filename, header.e_entry);

    if (header.e_entry == 0) {
        log_error(MODULE, "FATAL: ELF has entry point 0 (invalid)");
        if (node->type == VFS_FILE) kmem_free(node);
        return 0;
    }

    uint32_t phdr_size = header.e_phnum * header.e_phentsize;
    if (phdr_size == 0) {
        log_error(MODULE, "FATAL: ELF has no program headers");
        if (node->type == VFS_FILE) kmem_free(node);
        return 0;
    }

    elf32_phdr_t* phdrs = (elf32_phdr_t*)kmem_zalloc(phdr_size);
    if (phdrs == NULL) {
        log_error(MODULE, "FATAL: Failed to allocate program headers");
        if (node->type == VFS_FILE) kmem_free(node);
        return 0;
    }

    vfs_read(node, header.e_phoff, phdr_size, (uint8_t*)phdrs);

    // Create the new user page directory (no CR3 switching)
    uint32_t new_directory = (uint32_t)vmm_create_directory();
    log_debug(MODULE, "Using new page directory phys=0x%x (not switching CR3)", new_directory);

    // Load all program segments into the new directory
    for (int i = 0; i < header.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            uint32_t vaddr = phdrs[i].p_vaddr;
            uint32_t memsz = phdrs[i].p_memsz;
            uint32_t filesz = phdrs[i].p_filesz;
            uint32_t offset = phdrs[i].p_offset;

            uint32_t start_page = vaddr & ~0xFFF;
            uint32_t end_page = (vaddr + memsz + 0xFFF) & ~0xFFF;
            uint32_t page_count = (end_page - start_page) / 4096;

            log_debug(MODULE, "Loading PT_LOAD segment: vaddr=%x, memsz=%u, filesz=%u, pages=%u",
                      vaddr, memsz, filesz, page_count);

            uint32_t bytes_left_to_read = filesz;
            uint32_t file_offset_ptr = offset;

            for (uint32_t p = 0; p < page_count; p++) {
                uint32_t page_vaddr = start_page + (p * 4096);
                void* phys = pmm_alloc_block();

                // Read file data into a temporary buffer
                uint8_t temp_buf[4096];
                memset(temp_buf, 0, 4096);

                uint32_t read_len = 0;
                if (bytes_left_to_read > 0) {
                    read_len = (bytes_left_to_read > 4096) ? 4096 : bytes_left_to_read;
                    vfs_read(node, file_offset_ptr, read_len, temp_buf);
                    file_offset_ptr += read_len;
                    bytes_left_to_read -= read_len;
                }

                // Map the page in the *new* directory (no CR3 switch)
                vmm_map_page_in_directory(new_directory, phys, (void*)page_vaddr,
                                          PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

                // Copy data directly to the physical page (identity-mapped)
                uint32_t dest_offset = (page_vaddr < vaddr) ? (vaddr - page_vaddr) : 0;
                uint8_t* dest = (uint8_t*)phys + dest_offset;
                memset(phys, 0, 4096);          // zero the whole page
                if (read_len > 0) {
                    memcpy(dest, temp_buf, read_len);
                }
            }
        }
    }

    // Map the user stack into the new directory
    uint32_t user_stack_page = 0xBFFFF000;
    void* user_stack_phys = pmm_alloc_block();
    vmm_map_page_in_directory(new_directory, user_stack_phys, (void*)user_stack_page,
                              PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    memset(user_stack_phys, 0, 4096);   // zero the stack page

    // Set up initial user stack:
    // The stack top is 0xBFFFFFF0 (user_stack_page + 4096 - 16)
    uint32_t user_stack_top = user_stack_page + 4096 - 16;
    uint32_t* stack_top_ptr = (uint32_t*)((uint8_t*)user_stack_phys + 4096 - 16);

    // Write values at the correct offsets from the top
    stack_top_ptr[0] = header.e_entry;  // return address (entry point, so ret loops)
    stack_top_ptr[-1] = 0;              // argc = 0
    stack_top_ptr[-2] = 0;              // argv = NULL

    log_debug(MODULE, "User stack top physical addr: 0x%x, contents: ret=%x argc=%x argv=%x",
              (uint32_t)stack_top_ptr, stack_top_ptr[0], stack_top_ptr[-1], stack_top_ptr[-2]);

    // Verify PDEs by reading the new directory directly (not via CR3)
    uint32_t* new_dir_ptr = (uint32_t*)new_directory;
    log_debug(MODULE, "Verifying new directory PDE[32] = 0x%x", new_dir_ptr[32]);
    log_debug(MODULE, "Verifying new directory PDE[767] = 0x%x", new_dir_ptr[767]);

    kmem_free(phdrs);
    if (node->type == VFS_FILE) {
        kmem_free(node);
    }

    // Print entry bytes using multiple %x (no width specifier)
    uint32_t pd_index = (header.e_entry >> 22);
    uint32_t pt_index = (header.e_entry >> 12) & 0x3FF;
    if (new_dir_ptr[pd_index] & PAGE_PRESENT) {
        uint32_t table_phys = new_dir_ptr[pd_index] & ~0xFFF;
        uint32_t* table = (uint32_t*)table_phys;
        uint32_t entry_phys = table[pt_index] & ~0xFFF;
        uint8_t* bytes = (uint8_t*)entry_phys;
        log_debug(MODULE, "Entry bytes: %x %x %x %x", bytes[0], bytes[1], bytes[2], bytes[3]);
    } else {
        log_error(MODULE, "Entry page not mapped!");
    }

    log_debug(MODULE, "Creating user task: entry=%x, stack=%x, dir=%x",
              header.e_entry, user_stack_top, new_directory);

    keyboard_flush_buffer();

    uint32_t pid = task_create_user(filename, header.e_entry, new_directory);
    log_debug(MODULE, "User task created with PID: %u", pid);

    return pid;
}