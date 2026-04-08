#include "syscall.h"
#include "../kernel_services/kernel_services.h"
#include "../utils/logging/logger.h"
#include "../task/task.h"
#include <stdint.h>

#define MODULE "SYSCALL"

void syscall_dispatcher(cpu_state_t* regs) {
    uint32_t syscall_number = regs->eax;

    switch (syscall_number) {
        case 1: {
            kprintf((const char*)regs->ebx);
            break;
        }
        case 2: {
            task_yield();
            break;
        }
        default: {
            log_warning(MODULE, "Unknown Syscall: %d", syscall_number);
            break;
        }
    }
}