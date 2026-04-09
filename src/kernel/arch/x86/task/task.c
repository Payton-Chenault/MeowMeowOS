#include "task.h"
#include "../../../kernel_services/kernel_services.h"
#include "../../../lib/string/string.h"
#include "../interrupt_descriptor_table/idt.h"
#include "../../../mem/virtual_memory_manager/vmm.h"
#include "../../../utils/logging/logger.h"

#define MODULE "TASK"

extern void switch_to_task(uint32_t* old_esp, uint32_t new_esp, uint32_t new_cr3);

static task_t* current_task = NULL;
static task_t* task_list = NULL;
static uint32_t next_pid = 1;

static void idle_task_function(void) {
    while(1) {
        disable_interrupts();

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
        enable_interrupts();
        wait_for_interrupt();
    }
}

void task_initialize() {
    task_t* root_task = (task_t*)kmem_zalloc(sizeof(task_t));

    root_task->pid = next_pid++;
    strcpy(root_task->name, "kernel_shell");
    root_task->state = TASK_STATE_RUNNING;
    root_task->page_directory = (uint32_t)vmm_get_directory();

    root_task->fd_table[0].node = vfs_find("stdin");
    root_task->fd_table[1].node = vfs_find("stdout");
    root_task->fd_table[2].node = vfs_find("stdout");

    task_list = root_task;
    current_task = root_task;

    task_create("idle", idle_task_function);

    log_info(MODULE, "Initialized");
}

uint32_t task_create(const char* name, void (*entry_point)(void)) {
    task_t* new_task = (task_t*)kmem_zalloc(sizeof(task_t));

    uint32_t* stack = (uint32_t*)kmem_zalloc(4096);
    uint32_t* esp = (uint32_t*)((uint32_t)stack + 4096);

    new_task->stack_base = (uint32_t)stack;

    new_task->fd_table[0].in_use = true;
    strcpy(new_task->fd_table[0].filename, "stdin");
    new_task->fd_table[0].node = vfs_find("stdin");

    new_task->fd_table[1].in_use = true;
    strcpy(new_task->fd_table[1].filename, "stdout");
    new_task->fd_table[1].node = vfs_find("stdout");

    new_task->fd_table[2].in_use = true;
    strcpy(new_task->fd_table[2].filename, "stderr");
    new_task->fd_table[2].node = vfs_find("stdout");

    for (int i = 3; i < MAX_OPEN_FILES; i++ ){
        new_task->fd_table[i].in_use = false;
        new_task->fd_table[i].current_offset = 0;
        new_task->fd_table[i].file_size = 0;
        for(int j = 0; j < 32; j++) {
            new_task->fd_table[i].filename[j] = '\0';
        }
    }


    *(--esp) = (uint32_t)task_exit;

    *(--esp) = (uint32_t)entry_point;   // EIP
    *(--esp) = 0;                       // EBP
    *(--esp) = 0;                       // EBX
    *(--esp) = 0;                       // ESI
    *(--esp) = 0;                       // EDI

    new_task->pid = next_pid++;
    strcpy(new_task->name, name);
    new_task->esp = (uint32_t)esp;
    new_task->state = TASK_STATE_READY;

    new_task->page_directory = (uint32_t)vmm_get_directory();

    new_task->next = task_list;
    task_list = new_task;

    log_debug(MODULE, "Created task %s: (PID: %d)", name, new_task->pid);
    
    return new_task->pid;
}

void task_yield() {
    disable_interrupts();

    if (!current_task) {
        enable_interrupts();
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
        switch_to_task(&old->esp, current_task->esp, current_task->page_directory);
    }

    enable_interrupts();
}

void task_exit() {
    disable_interrupts();
    current_task->state = TASK_STATE_DEAD;

    task_t* temp = task_list;
    while (temp) {
        if (temp->state == TASK_STATE_WAITING && temp->waiting_on_pid == current_task->pid) {
            temp->state = TASK_STATE_READY;
            temp->waiting_on_pid = 0;
        }
        temp = temp->next;
    }

    while (1) {
        task_yield();
    }
}

void task_wait(uint32_t pid) {
    disable_interrupts();

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
        task_yield();
    }

    enable_interrupts();
}

task_t* task_get_current(void) {
    return current_task;
}