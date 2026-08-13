#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stdbool.h>
#include "../../../fs/vfs/vfs.h"


#define TASK_STATE_READY 0
#define TASK_STATE_RUNNING 1
#define TASK_STATE_WAITING 2
#define TASK_STATE_DEAD 3

#define MAX_OPEN_FILES 16

typedef struct {
    bool in_use;
    char filename[32];
    uint32_t current_offset;
    uint32_t file_size;

    struct vfs_node* node;
} file_descriptor_t;

typedef struct {
    uint32_t es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t eip, cs, eflags;
} cpu_state_t;

typedef struct task {
    uint32_t pid;
    char name[32];
    uint32_t esp;
    uint32_t page_directory;
    uint32_t uid; 
    uint32_t stack_base;
    uint32_t kernel_stack_top; 
    uint8_t state;
    uint32_t waiting_on_pid;
    uint32_t user_rip;
    uint32_t is_user;
    file_descriptor_t fd_table[MAX_OPEN_FILES];
    struct task* next;
} task_t;

void task_initialize(void);
uint32_t task_create_user(const char* name, uint32_t entry_point, uint32_t page_directory);
uint32_t task_create(const char* name, void (*entry_point)(void),  uint32_t page_directory);
void task_yield(void);
void task_wait(uint32_t pid);
void task_exit(void);
task_t* task_get_current(void);

extern void enter_ring3(uint32_t entry_point, uint32_t user_stack);

#endif