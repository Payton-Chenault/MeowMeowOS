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

uint32_t elf_load_and_spawn(const char *filename, int argc, char **argv) {
    log_debug(MODULE, "ELF loader: trying to open %s", filename);

    vfs_node_t *node = vfs_find(filename);
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
        if (node->type == VFS_FILE)
            kmem_free(node);
        return 0;
    }

    elf32_ehdr_t header;
    if (vfs_read(node, 0, sizeof(elf32_ehdr_t), (uint8_t *)&header) == 0) {
        log_error(MODULE, "FATAL: Failed to read ELF header");
        if (node->type == VFS_FILE)
            kmem_free(node);
        return 0;
    }

    uint32_t magic = *(uint32_t *)header.e_ident;
    if (magic != ELF_MAGIC || header.e_type != 2) {
        log_error(MODULE, "FATAL: Invalid ELF executable: %s", filename);
        if (node->type == VFS_FILE)
            kmem_free(node);
        return 0;
    }

    log_info(MODULE, "OK: Valid ELF found: %s, Entry point: %x", filename, header.e_entry);

    if (header.e_entry == 0) {
        log_error(MODULE, "FATAL: ELF has entry point 0 (invalid)");
        if (node->type == VFS_FILE)
            kmem_free(node);
        return 0;
    }

    uint32_t phdr_size = header.e_phnum * header.e_phentsize;
    if (phdr_size == 0) {
        log_error(MODULE, "FATAL: ELF has no program headers");
        if (node->type == VFS_FILE)
            kmem_free(node);
        return 0;
    }

    elf32_phdr_t *phdrs = (elf32_phdr_t *)kmem_zalloc(phdr_size);
    if (phdrs == NULL) {
        log_error(MODULE, "FATAL: Failed to allocate program headers");
        if (node->type == VFS_FILE)
            kmem_free(node);
        return 0;
    }

    vfs_read(node, header.e_phoff, phdr_size, (uint8_t *)phdrs);

    // Create new page directory for the user process
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

            for (uint32_t p = 0; p < page_count; p++) {
                uint32_t page_vaddr = start_page + (p * PAGE_SIZE);
                void *phys = pmm_alloc_block();

                if (phys == NULL) {
                    log_error(MODULE, "FATAL: Out of physical memory loading %s", filename);
                    kmem_free(phdrs);
                    if (node->type == VFS_FILE)
                        kmem_free(node);
                    return 0;
                }

                memset(phys, 0, PAGE_SIZE);

                vmm_map_page_in_directory(new_directory, phys, (void *)page_vaddr,
                                          PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

                uint32_t dest_offset = (page_vaddr < vaddr) ? (vaddr - page_vaddr) : 0;
                uint32_t segment_offset = (page_vaddr < vaddr) ? 0 : (page_vaddr - vaddr);
                uint32_t read_len = 0;
                if (segment_offset < filesz) {
                    uint32_t bytes_available = filesz - segment_offset;
                    uint32_t page_capacity = PAGE_SIZE - dest_offset;
                    read_len = (bytes_available < page_capacity) ? bytes_available : page_capacity;
                }

                if (read_len > 0) {
                    vfs_read(node, offset + segment_offset, read_len,
                             (uint8_t *)phys + dest_offset);
                }
            }
        }
    }

    // Map the user stack
    uint32_t user_stack_page = 0xBFFFF000;
    void *user_stack_phys = pmm_alloc_block();
    if (user_stack_phys == NULL) {
        log_error(MODULE, "FATAL: Out of memory for user stack");
        kmem_free(phdrs);
        if (node->type == VFS_FILE)
            kmem_free(node);
        return 0;
    }
    vmm_map_page_in_directory(new_directory, user_stack_phys, (void *)user_stack_page,
                              PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    memset(user_stack_phys, 0, 4096);

    uint32_t user_tls_page = 0xBFFFE000;  // just below stack
    void *tls_phys = pmm_alloc_block();
    if (tls_phys == NULL) {
        log_error(MODULE, "FATAL: Out of memory for TLS page");
        kmem_free(phdrs);
        if (node->type == VFS_FILE)
            kmem_free(node);
        return 0;
    }
    vmm_map_page_in_directory(new_directory, tls_phys, (void *)user_tls_page,
                              PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    memset(tls_phys, 0, PAGE_SIZE);

    // User stack top = 0xBFFFFFF0 (16 bytes before end of page)
    uint32_t stack_base = user_stack_page;          // 0xBFFFF000
    uint32_t stack_esp  = stack_base + 4096 - 16;   // 0xBFFFFFF0
    uint8_t *phys_base   = (uint8_t *)user_stack_phys;


    uint32_t str_off = 4096 - 256;   // 0xF00
    uint32_t str_pos = str_off;
    uint32_t argv_ptrs[64];
    if (argc > 63)
        argc = 63;

    for (int i = 0; i < argc; i++) {
        size_t len = strlen(argv[i]) + 1;
        if (str_pos + len > 4096 - 16)
            break;

        memcpy(phys_base + str_pos, argv[i], len);
        argv_ptrs[i] = stack_base + str_pos;
        str_pos += len;
    }
    argv_ptrs[argc] = 0;  // null terminator

    // Align pointer array upward
    uint32_t ptr_array_off = (str_pos + 3) & ~3u;
    if (ptr_array_off + (argc + 1) * 4 > 4096 - 16) {
        ptr_array_off = 4096 - 16 - (argc + 1) * 4;
    }
    uint32_t *ptr_array = (uint32_t *)(phys_base + ptr_array_off);
    for (int i = 0; i <= argc; i++) {
        ptr_array[i] = argv_ptrs[i];
    }

    uint32_t argv_virtual = stack_base + ptr_array_off;

    uint32_t *stack_ptr = (uint32_t *)(phys_base + (stack_esp - stack_base));
    stack_ptr[0] = (uint32_t)argc;
    stack_ptr[1] = argv_virtual;

    log_debug(MODULE, "User stack: esp=%x argc=%d argv=%x",
              stack_esp, argc, argv_virtual);

    // Flush keyboard buffer before launching program
    keyboard_flush_buffer();

    kmem_free(phdrs);
    if (node->type == VFS_FILE) {
        kmem_free(node);
    }

    log_debug(MODULE, "Creating user task: entry=%x, stack=%x, dir=%x",
              header.e_entry, stack_esp, new_directory);

    uint32_t pid = task_create_user(filename, header.e_entry, new_directory);
    log_debug(MODULE, "User task created with PID: %u", pid);

    if (pid != 0) {
        task_t *task = task_get_by_pid(pid);
        if (task) {
            task->tls_ptr = user_tls_page;
            log_debug(MODULE, "Set TLS pointer for PID %u to 0x%x", pid, user_tls_page);
        }
    }

    return pid;
}

uint32_t elf_get_description(const char *filename, char *buffer, uint32_t size)
{
    log_info("ELF_DESC", "called with '%s'", filename);

    if (filename == NULL || buffer == NULL || size == 0) {
        log_info("ELF_DESC", "invalid args");
        return 0;
    }

    vfs_node_t *node = fat16_vfs_open(filename);
    if (node == NULL) {
        if (filename[0] == '/')
            node = fat16_vfs_open(filename + 1);
        if (node == NULL && filename[0] != '/') {
            char abs_path[256];
            if (strlen(filename) + 2 < sizeof(abs_path)) {
                abs_path[0] = '/';
                strcpy(abs_path + 1, filename);
                node = fat16_vfs_open(abs_path);
            }
        }
    }

    if (node == NULL) {
        log_info("ELF_DESC", "open failed for '%s'", filename);
        return 0;
    }
    log_info("ELF_DESC", "file opened");

    elf32_ehdr_t eh;
    int bytes_read = vfs_read(node, 0, sizeof(elf32_ehdr_t), (uint8_t *)&eh);
    if (bytes_read != sizeof(elf32_ehdr_t)) {
        log_info("ELF_DESC", "header read failed (got %d)", bytes_read);
        vfs_close(node);
        return 0;
    }

    uint32_t magic = *(uint32_t *)eh.e_ident;
    if (magic != ELF_MAGIC) {
        log_info("ELF_DESC", "bad magic");
        vfs_close(node);
        return 0;
    }

    if (eh.e_shoff == 0 || eh.e_shnum == 0 || eh.e_shstrndx >= eh.e_shnum) {
        log_info("ELF_DESC", "no section headers (shnum=%u)", eh.e_shnum);
        vfs_close(node);
        return 0;
    }

    elf32_shdr_t shstr_hdr;
    uint32_t shstr_off = eh.e_shoff + eh.e_shstrndx * sizeof(elf32_shdr_t);
    bytes_read = vfs_read(node, shstr_off, sizeof(elf32_shdr_t), (uint8_t *)&shstr_hdr);
    if (bytes_read != sizeof(elf32_shdr_t)) {
        log_info("ELF_DESC", "shstr header read failed");
        vfs_close(node);
        return 0;
    }

    if (shstr_hdr.sh_size == 0) {
        vfs_close(node);
        return 0;
    }

    char *shstr = (char *)kmem_alloc(shstr_hdr.sh_size);
    if (shstr == NULL) {
        log_info("ELF_DESC", "kmem_alloc failed");
        vfs_close(node);
        return 0;
    }

    bytes_read = vfs_read(node, shstr_hdr.sh_offset, shstr_hdr.sh_size, (uint8_t *)shstr);
    if (bytes_read != shstr_hdr.sh_size) {
        log_info("ELF_DESC", "shstr read failed (got %d)", bytes_read);
        kmem_free(shstr);
        vfs_close(node);
        return 0;
    }

    uint32_t found = 0;
    for (uint16_t i = 0; i < eh.e_shnum; i++) {
        elf32_shdr_t sh;
        uint32_t off = eh.e_shoff + i * sizeof(elf32_shdr_t);
        bytes_read = vfs_read(node, off, sizeof(elf32_shdr_t), (uint8_t *)&sh);
        if (bytes_read != sizeof(elf32_shdr_t)) continue;
        if (sh.sh_name >= shstr_hdr.sh_size) continue;

        const char *name = shstr + sh.sh_name;
        if (strcmp(name, ".description") == 0) {
            uint32_t copy = sh.sh_size;
            if (copy > size - 1) copy = size - 1;
            bytes_read = vfs_read(node, sh.sh_offset, copy, (uint8_t *)buffer);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                found = 1;
                log_info("ELF_DESC", "description found: '%s'", buffer);
            } else {
                log_info("ELF_DESC", "read description returned %d", bytes_read);
            }
            break;
        }
    }

    kmem_free(shstr);
    vfs_close(node);
    log_info("ELF_DESC", "returning %d", found);
    return found;
}