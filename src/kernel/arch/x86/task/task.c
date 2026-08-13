#include "task.h"
#include "../../../kernel_services/kernel_services.h"
#include "../../../lib/string/string.h"
#include "../global_descriptor_table/gdt.h"
#include "../interrupt_descriptor_table/idt.h"
#include "../../../mem/virtual_memory_manager/vmm.h"
#include "../../../utils/logging/logger.h"
#include "../sync/spinlock.h"
#include <stdint.h>

#define MODULE "TASK"

extern void switch_to_task(uint32_t* old_esp, uint32_t new_esp, uint32_t new_cr3);
extern void switch_to_user_task(uint32_t* old_esp, uint32_t new_esp, uint32_t new_cr3);

static task_t* current_task = NULL;
static task_t* task_list = NULL;
static uint32_t next_pid = 1;
static spinlock_t task_lock = SPINLOCK_INIT;

static bool needs_cleanup = false;

static void idle_task_function(void) {
    while(1) {
        if (needs_cleanup) {
            spinlock_acquire_irq(&task_lock);

            task_t* prev = NULL;
            task_t* current = task_list;
       
            while (current != NULL) {
                if(current->state == TASK_STATE_DEAD) {
                    if(prev != NULL) {
                        prev->next = current->next;
                    } else {
                        task_list = current->next;
                    }

                    task_t* task_to_delete = current;
                    current = current->next;

                    kmem_free((void*)task_to_delete->stack_base);
                    kmem_free((void*)task_to_delete);
                } else {
                    prev = current;
                    current = current->next;
                }
            }

            needs_cleanup = false;
            spinlock_release_irq(&task_lock);
        }

        wait_for_interrupt();
    }
}

void task_initialize() {
    task_t* root_task = (task_t*)kmem_zalloc(sizeof(task_t));

    root_task->pid = next_pid++;
    strcpy(root_task->name, "kernel_shell");
    root_task->state = TASK_STATE_RUNNING;
    root_task->page_directory = (uint32_t)vmm_get_directory();
    root_task->is_user = false;

    root_task->fd_table[0].node = vfs_find("stdin");
    root_task->fd_table[1].node = vfs_find("stdout");
    root_task->fd_table[2].node = vfs_find("stdout");

    uint32_t* root_stack = (uint32_t*)kmem_zalloc(16384);
    root_task->stack_base = (uint32_t)root_stack;
    root_task->kernel_stack_top = (uint32_t)root_stack + 16384;
    root_task->esp = root_task->kernel_stack_top;

    tss_set_kernel_stack(root_task->kernel_stack_top);

    task_list = root_task;
    current_task = root_task;

    task_create("idle", idle_task_function, (uint32_t)vmm_get_directory());

    log_info(MODULE, "Initialized");
}

uint32_t task_create(const char* name, void (*entry_point)(void), uint32_t page_directory) {
    task_t* new_task = (task_t*)kmem_zalloc(sizeof(task_t));

    uint32_t* stack = (uint32_t*)kmem_zalloc(16384);
    uint32_t* esp = (uint32_t*)((uint32_t)stack + 16384);

    new_task->stack_base = (uint32_t)stack;
    new_task->kernel_stack_top = (uint32_t)stack + 16384;
    new_task->is_user = false;

    new_task->fd_table[0].in_use = true;
    new_task->fd_table[0].node = vfs_find("stdin");
    new_task->fd_table[1].in_use = true;
    new_task->fd_table[1].node = vfs_find("stdout");
    new_task->fd_table[2].in_use = true;
    new_task->fd_table[2].node = vfs_find("stdout");

    for (int i = 3; i < MAX_OPEN_FILES; i++ ){
        new_task->fd_table[i].in_use = false;
    }

    *(--esp) = (uint32_t)task_exit;
    *(--esp) = (uint32_t)entry_point;
    *(--esp) = 0;
    *(--esp) = 0;
    *(--esp) = 0;
    *(--esp) = 0;

    new_task->pid = next_pid++;
    strcpy(new_task->name, name);
    new_task->esp = (uint32_t)esp;
    new_task->state = TASK_STATE_READY;
    new_task->page_directory = page_directory;

    spinlock_acquire(&task_lock);
    new_task->next = task_list;
    task_list = new_task;
    spinlock_release(&task_lock);

    log_debug(MODULE, "Created kernel task %s: (PID: %d)", name, new_task->pid);
    return new_task->pid;
}

uint32_t task_create_user(const char* name, uint32_t entry_point, uint32_t page_directory) {
    task_t* new_task = (task_t*)kmem_zalloc(sizeof(task_t));

    uint32_t* kstack = (uint32_t*)kmem_zalloc(16384);
    uint32_t stack_top = (uint32_t)kstack + 16384;
    stack_top &= ~0xF;   // 16-byte align

    new_task->stack_base = (uint32_t)kstack;
    new_task->kernel_stack_top = stack_top;
    new_task->is_user = true;

    // Build iret frame
    uint32_t* esp = (uint32_t*)stack_top;

    *(--esp) = 0x23;                        // SS
    *(--esp) = 0xBFFFF000 + 4096 - 16;      // ESP
    *(--esp) = 0x202;                       // EFLAGS
    *(--esp) = 0x1B;                        // CS
    *(--esp) = entry_point;                 // EIP

    new_task->esp = (uint32_t)esp;

    new_task->fd_table[0].in_use = true;
    new_task->fd_table[0].node = vfs_find("stdin");
    new_task->fd_table[1].in_use = true;
    new_task->fd_table[1].node = vfs_find("stdout");
    new_task->fd_table[2].in_use = true;
    new_task->fd_table[2].node = vfs_find("stdout");
    for (int i = 3; i < MAX_OPEN_FILES; i++) {
        new_task->fd_table[i].in_use = false;
    }

    new_task->pid = next_pid++;
    strcpy(new_task->name, name);
    new_task->state = TASK_STATE_READY;
    new_task->page_directory = page_directory;

    spinlock_acquire(&task_lock);
    new_task->next = task_list;
    task_list = new_task;
    spinlock_release(&task_lock);

    log_debug(MODULE, "Created user task %s: (PID: %d), esp=%x", name, new_task->pid, new_task->esp);
    return new_task->pid;
}

void task_yield() {
    uint32_t flags = spinlock_acquire_irq_save(&task_lock);

    if (!current_task) {
        spinlock_release_irq_restore(&task_lock, flags);
        return;
    }

    task_t* next = current_task->next;
    if (!next) {
        next = task_list; 
    }

    while(1) {
        if (!next) next = task_list;
        if (next == current_task) break;

        if (next->state == TASK_STATE_READY || next->state == TASK_STATE_RUNNING) {
            break;
        }
        next = next->next;
    }

    if (next != current_task && (next->state == TASK_STATE_READY || next->state == TASK_STATE_RUNNING)) {
        task_t* old = current_task;
        current_task = next;
        
        spinlock_release(&task_lock);
        
        tss_set_kernel_stack(current_task->kernel_stack_top);
        
        if (current_task->is_user) {
            switch_to_user_task(&old->esp, current_task->esp, current_task->page_directory);
        } else {
            switch_to_task(&old->esp, current_task->esp, current_task->page_directory);
        }
    } else {
        spinlock_release_irq_restore(&task_lock, flags);
    }
}

void task_exit() {
    spinlock_acquire_irq(&task_lock);
    current_task->state = TASK_STATE_DEAD;

    task_t* temp = task_list;
    while (temp) {
        if (temp->state == TASK_STATE_WAITING && temp->waiting_on_pid == current_task->pid) {
            temp->state = TASK_STATE_READY;
            temp->waiting_on_pid = 0;
        }
        temp = temp->next;
    }

    for (int i = 3; i < MAX_OPEN_FILES; i++) {
        if (current_task->fd_table[i].in_use) {
            vfs_node_t* node = current_task->fd_table[i].node;
            if (node != NULL && node->type == VFS_FILE) {
                kmem_free(node);
            }
            current_task->fd_table[i].in_use = false;
        }
    }

    needs_cleanup = true;

    spinlock_release(&task_lock);

    while (1) {
        task_yield();
    }
}

void task_wait(uint32_t pid) {
    spinlock_acquire_irq(&task_lock);

    bool found = false;
    task_t* temp = task_list;

    while(temp) {
        if(temp->pid == pid && temp->state != TASK_STATE_DEAD) {
            found = true;
            break;
        }
        temp = temp->next;
    }

    if (found) {
        current_task->state = TASK_STATE_WAITING;
        current_task->waiting_on_pid = pid;
        spinlock_release(&task_lock);
        task_yield();
    } else {
        spinlock_release(&task_lock);
    }

    enable_interrupts();
}

task_t* task_get_current(void) {
    return current_task;
}