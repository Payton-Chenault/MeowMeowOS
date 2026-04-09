#include "syscall.h"
#include "../kernel_services/kernel_services.h"
#include "../utils/logging/logger.h"
#include "../arch/x86/task/task.h"
#include "../drivers/keyboard/keyboard.h"
#include "../fs/fat_16/fat16.h"
#include "../fs/fat_16/fat16_vfs.h"
#include "../lib/string/string.h"
#include <stdint.h>

#define MODULE "SYSCALL"

void syscall_dispatcher(cpu_state_t* regs) {
    uint32_t syscall_number = regs->eax;

    switch (syscall_number) {
        case SYS_YIELD: {
            task_yield();
            break;
        }
        case SYS_RETURN: {
            task_exit();
            break;
        }
case SYS_OPEN: {
            const char* filename = (const char*)regs->ecx;
            task_t* current = task_get_current();
            
            // 1. Is this a permanent device like "stdin" or "stdout"?
            vfs_node_t* target_node = vfs_find(filename);

            // 2. If it's not a device, ask the FAT16 VFS to find it on the disk!
            if (target_node == NULL) {
                target_node = fat16_vfs_open(filename);
            }

            // 3. If it's STILL NULL, the file simply doesn't exist.
            if (target_node == NULL) {
                regs->eax = -1; 
                break;
            }

            // 4. Find an empty slot in the FD Table
            int free_fd = -1;
            for (int i = 0; i < MAX_OPEN_FILES; i++) {
                if (current->fd_table[i].in_use == false) {
                    free_fd = i;
                    break;
                }
            }

            // 5. Save the VFS Node into the FD Table!
            if (free_fd != -1) {
                current->fd_table[free_fd].in_use = true;
                strcpy(current->fd_table[free_fd].filename, filename);
                current->fd_table[free_fd].file_size = target_node->length;
                current->fd_table[free_fd].current_offset = 0;
                
                // Make sure your task.h file_descriptor_t struct has this pointer!
                current->fd_table[free_fd].node = target_node; 
            }

            regs->eax = free_fd;
            break;
        }
        case SYS_READ: {
            int fd = (int)regs->ecx;
            uint8_t* buffer = (uint8_t*)regs->edx;
            uint32_t bytes_to_read = regs->esi;

            task_t* current = task_get_current();

            // Sanity Check
            if (fd < 0 || fd >= MAX_OPEN_FILES || current->fd_table[fd].in_use == false) {
                regs->eax = -1;
                break;
            }

            vfs_node_t* node = current->fd_table[fd].node;
            uint32_t current_offset = current->fd_table[fd].current_offset;

            // THE MAGIC: Call the universal VFS read!
            uint32_t bytes_read = vfs_read(node, current_offset, bytes_to_read, buffer);
            
            // Update the tracker
            current->fd_table[fd].current_offset += bytes_read;

            regs->eax = bytes_read;
            break;
        }
        case SYS_WRITE: {
            int fd = (int)regs->ecx;
            uint8_t* buffer = (uint8_t*)regs->edx;
            uint32_t bytes_to_write = regs->esi;

            task_t* current = task_get_current();

            // Sanity Check
            if (fd < 0 || fd >= MAX_OPEN_FILES || current->fd_table[fd].in_use == false) {
                regs->eax = -1;
                break;
            }

            vfs_node_t* node = current->fd_table[fd].node;
            uint32_t current_offset = current->fd_table[fd].current_offset;

            // THE MAGIC: Call the universal VFS write!
            uint32_t bytes_written = vfs_write(node, current_offset, bytes_to_write, buffer);
            
            // Update the tracker
            current->fd_table[fd].current_offset += bytes_written;

            regs->eax = bytes_written;
            break;
        }
        default: {
            log_warning(MODULE, "Unknown Syscall: %d", syscall_number);
            break;
        }
    }
}