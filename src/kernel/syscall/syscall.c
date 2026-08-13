#include "syscall.h"
#include "../kernel_services/kernel_services.h"
#include "../utils/logging/logger.h"
#include "../arch/x86/task/task.h"
#include "../security/auth/auth.h"
#include "../fs/fat_16/fat16.h"
#include "../fs/fat_16/fat16_vfs.h"
#include "../lib/string/string.h"
#include <stdint.h>

extern int task_is_root(void);

#define MODULE "SYSCALL"

// Correct stack layout after `pusha; push ds; push es`
typedef struct {
    uint32_t es;
    uint32_t ds;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;   // original ESP before pusha
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;   // syscall number and return value
} syscall_regs_t;

void syscall_dispatcher(syscall_regs_t* regs) {
    uint32_t syscall_number = regs->eax;

    log_debug(MODULE, "Syscall %d: ebx=%x ecx=%x edx=%x esi=%x",
              syscall_number, regs->ebx, regs->ecx, regs->edx, regs->esi);

    switch (syscall_number) {
        case SYS_YIELD:
            log_debug(MODULE, "Syscall: yield");
            task_yield();
            regs->eax = 0;
            break;

        case SYS_RETURN:
            log_debug(MODULE, "Syscall: return/exit");
            task_exit();
            regs->eax = 0;
            break;

        case SYS_OPEN: {
            const char* filename = (const char*)regs->ebx;
            log_debug(MODULE, "Syscall open: filename=%s", filename);
            if (filename == NULL) {
                log_error(MODULE, "SYS_OPEN: null filename");
                regs->eax = -1;
                break;
            }

            task_t* current = task_get_current();
            vfs_node_t* target_node = vfs_find(filename);
            if (!target_node) target_node = fat16_vfs_open(filename);

            if (!target_node) {
                log_info(MODULE, "File '%s' not found. Auto-creating...", filename);
                fat16_write_file(filename, (uint8_t*)" ", 1);
                target_node = fat16_vfs_open(filename);
                if (!target_node) {
                    log_error(MODULE, "Failed to auto-create file: %s", filename);
                    regs->eax = -1;
                    break;
                }
            }

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
                current->fd_table[free_fd].file_size = target_node->length;
                current->fd_table[free_fd].current_offset = 0;
                current->fd_table[free_fd].node = target_node;
                regs->eax = free_fd;
                log_debug(MODULE, "SYS_OPEN success: fd=%d", free_fd);
            } else {
                log_error(MODULE, "FATAL: Task out of FD slots!");
                if (target_node->type == VFS_FILE) kmem_free(target_node);
                regs->eax = -1;
            }
            break;
        }

        case SYS_CLOSE: {
            int fd = (int)regs->ebx;
            log_debug(MODULE, "Syscall close: fd=%d", fd);
            task_t* current = task_get_current();
            if (fd < 0 || fd >= MAX_OPEN_FILES || !current->fd_table[fd].in_use) {
                log_error(MODULE, "SYS_CLOSE: invalid fd %d", fd);
                regs->eax = -1;
                break;
            }

            vfs_node_t* node = current->fd_table[fd].node;
            if (node && node->type == VFS_FILE) {
                log_debug(MODULE, "Closing and freeing node: %s", node->name);
                kmem_free(node);
            }

            current->fd_table[fd].in_use = false;
            current->fd_table[fd].node = NULL;
            memset(current->fd_table[fd].filename, 0, 32);
            current->fd_table[fd].current_offset = 0;

            regs->eax = 0;
            log_debug(MODULE, "SYS_CLOSE success");
            break;
        }

        case SYS_READ: {
            int fd = (int)regs->ebx;
            uint8_t* buffer = (uint8_t*)regs->ecx;
            uint32_t bytes_to_read = regs->edx;

            log_debug(MODULE, "Syscall read: fd=%d buffer=%x size=%u", fd, buffer, bytes_to_read);

            if (!buffer) {
                log_error(MODULE, "SYS_READ: null buffer");
                regs->eax = -1;
                break;
            }

            task_t* current = task_get_current();
            if (fd < 0 || fd >= MAX_OPEN_FILES || !current->fd_table[fd].in_use) {
                log_error(MODULE, "SYS_READ: invalid fd %d", fd);
                regs->eax = -1;
                break;
            }

            vfs_node_t* node = current->fd_table[fd].node;
            uint32_t off = current->fd_table[fd].current_offset;
            uint32_t bytes_read = vfs_read(node, off, bytes_to_read, buffer);
            current->fd_table[fd].current_offset += bytes_read;

            // Echo input from stdin (fd 0) to stdout
            if (fd == 0 && bytes_read > 0) {
                vfs_node_t* stdout_node = vfs_find("stdout");
                if (stdout_node) {
                    vfs_write(stdout_node, 0, bytes_read, buffer);
                }
            }

            regs->eax = bytes_read;
            log_debug(MODULE, "SYS_READ success: %u bytes", bytes_read);
            break;
        }

        case SYS_WRITE: {
            int fd = (int)regs->ebx;
            uint8_t* buffer = (uint8_t*)regs->ecx;
            uint32_t bytes_to_write = regs->edx;

            log_debug(MODULE, "Syscall write: fd=%d buffer=%x size=%u", fd, buffer, bytes_to_write);
            if (!buffer) {
                log_error(MODULE, "SYS_WRITE: null buffer");
                regs->eax = -1;
                break;
            }

            task_t* current = task_get_current();
            if (fd < 0 || fd >= MAX_OPEN_FILES || !current->fd_table[fd].in_use) {
                log_error(MODULE, "SYS_WRITE: invalid fd %d", fd);
                regs->eax = -1;
                break;
            }

            vfs_node_t* node = current->fd_table[fd].node;
            uint32_t off = current->fd_table[fd].current_offset;
            uint32_t bytes_written = vfs_write(node, off, bytes_to_write, buffer);
            current->fd_table[fd].current_offset += bytes_written;

            regs->eax = bytes_written;
            log_debug(MODULE, "SYS_WRITE success: %u bytes", bytes_written);
            break;
        }

        case SYS_FORMAT:
            log_debug(MODULE, "Syscall format");
            if (!task_is_root()) {
                log_error(MODULE, "SYS_FORMAT: not root");
                regs->eax = (uint32_t)-1;
                break;
            }
            fat16_format_drive(0x80, 0, NULL);
            regs->eax = 0;
            log_debug(MODULE, "SYS_FORMAT success");
            break;

        default:
            log_warning(MODULE, "Unknown Syscall: %d", syscall_number);
            regs->eax = -1;
            break;
    }
}