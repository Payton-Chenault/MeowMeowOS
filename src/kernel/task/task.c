#include "task.h"
#include "../kernel_services/kernel_services.h"
#include "../lib/string/string.h"
#include "../arch/x86/interrupt_descriptor_table/idt.h"
#include "../mem/virtual_memory_manager/vmm.h"
#include "../utils/logging/logger.h"
#include <stdint.h>

#define MODULE "TASK"

extern void switch_to_task(uint32_t* old_esp, uint32_t new_esp, uint32_t new_cr3);

static void idle_task_function(void) {
    while(1) {
        __asm__ volatile("sti");
        __asm__ volatile("hlt");
    }
}

static task_t* current_task = NULL;
static task_t* task_list = NULL;
static uint32_t next_pid = 1;

void task_initialize() {
    task_t* root_task = (task_t*)kmem_zalloc(sizeof(task_t));

    root_task->pid = next_pid++;
    strcpy(root_task->name, "kernel_shell");
    root_task->state = TASK_STATE_RUNNING;
    root_task->page_directory = (uint32_t)vmm_get_directory();

    task_list = root_task;
    current_task = root_task;

    task_create("idle", idle_task_function);

    log_info(MODULE, "Initialized");
}

void task_create(const char* name, void (*entry_point)(void)) {
    task_t* new_task = (task_t*)kmem_zalloc(sizeof(task_t));

    uint32_t* stack = (uint32_t*)kmem_zalloc(4096);
    uint32_t* esp = (uint32_t*)((uint32_t)stack + 4096);


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

    if (next == current_task) {
        enable_interrupts();
        return;
    }

    task_t* old = current_task;
    current_task = next;

    // 3. Perform the actual context switch
    switch_to_task(&old->esp, current_task->esp, current_task->page_directory);

    enable_interrupts();
}