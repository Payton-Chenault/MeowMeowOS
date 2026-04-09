#include "syscall.h"
#include "../kernel_services/kernel_services.h"
#include "../utils/logging/logger.h"
#include "../arch/x86/task/task.h"
#include "../drivers/keyboard/keyboard.h"
#include <stdint.h>

#define MODULE "SYSCALL"

void syscall_dispatcher(cpu_state_t* regs) {
    uint32_t syscall_number = regs->eax;

    switch (syscall_number) {
        case SYS_PRINT: {
            kprintf((const char*)regs->ebx);
            break;
        }
        case SYS_YIELD: {
            task_yield();
            break;
        }
        case SYS_READ_CHAR:
            while (!keyboard_has_key()) {
                task_yield();
            }

            regs->eax = keyboard_read_char();
            break;
        case SYS_RETURN: {
            task_exit();
            break;
        }
        default: {
            log_warning(MODULE, "Unknown Syscall: %d", syscall_number);
            break;
        }
    }
}