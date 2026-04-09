#include "syscall.h"
#include "../kernel_services/kernel_services.h"
#include "../utils/logging/logger.h"
#include "../arch/x86/task/task.h"
#include "../drivers/keyboard/keyboard.h"
#include "../fs/fat_16/fat16.h"
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
            uint32_t file_size = fat16_get_file_size(filename);

            log_debug(MODULE, "sys_open requested: '%s' at Address: %x", filename, filename);            
            if (file_size == 0) {
                regs->eax = -1;
                break;
            }

            task_t* current = task_get_current();
            int free_fd = -1;

            for (int i = 0; i < MAX_OPEN_FILES; i++) {
                if (current->fd_table[i].in_use == false) {
                    free_fd = i;
                    break;
                }
            }

            if (free_fd != -1) {
                current->fd_table[free_fd].in_use = true;
                strcpy(current->fd_table[free_fd].filename, filename);
                current->fd_table[free_fd].file_size = file_size;
                current->fd_table[free_fd].current_offset = 0;
            }

            regs->eax = free_fd;
            break;
        }
        case SYS_READ: {
            int fd = (int)regs->ecx;
            uint8_t* buffer = (uint8_t*)regs->edx;
            uint32_t bytes_to_read = regs->esi;

            task_t* current = task_get_current();

            if (fd == 0) {
                if (bytes_to_read == 0) {
                    regs->eax = 0;
                    break;
                }

                while (!keyboard_has_key()) {
                    task_yield();
                }

                buffer[0] = keyboard_read_char();
                regs->eax = 1;
                break;
            }
            if (fd < 0 || fd >= MAX_OPEN_FILES || current->fd_table[fd].in_use == false) {
                regs->eax = -1;
                break;
            }

            uint32_t file_size = current->fd_table[fd].file_size;
            uint32_t current_offset = current->fd_table[fd].current_offset;

            if (current_offset >= file_size) {
                regs->eax = 0;
                break;
            }

            if(current_offset + bytes_to_read >= file_size) {
                bytes_to_read = file_size - current_offset;
            }

            uint8_t* temp_buffer = (uint8_t*)kmem_zalloc(file_size);
            fat16_read_file(current->fd_table[fd].filename, temp_buffer);

            memcpy(buffer, temp_buffer + current_offset, bytes_to_read);
            current->fd_table[fd].current_offset += bytes_to_read;
            kmem_free(temp_buffer);

            regs->eax = bytes_to_read;
            break;
        }
        case SYS_WRITE: {
            int fd = (int)regs->ecx;
            uint8_t* buffer = (uint8_t*)regs->edx;
            uint32_t bytes_to_write = regs->esi;

            if (fd == 1 || fd == 2) {
                char* temp_str = (char*)kmem_zalloc(bytes_to_write + 1);
                memcpy(temp_str, buffer, bytes_to_write);
                temp_str[bytes_to_write] = '\0';

                kprintf(temp_str);
                kmem_free(temp_str);

                regs->eax = bytes_to_write;
                break;
            }
        }
        default: {
            log_warning(MODULE, "Unknown Syscall: %d", syscall_number);
            break;
        }
    }
}