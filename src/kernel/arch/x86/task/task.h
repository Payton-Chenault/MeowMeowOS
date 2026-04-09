#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define TASK_STATE_READY 0
#define TASK_STATE_RUNNING 1
#define TASK_STATE_WAITING 2
#define TASK_STATE_DEAD 3

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
    uint8_t state;
    uint32_t waiting_on_pid;
    uint32_t stack_base;
    struct task* next;
} task_t;

void task_initialize(void);
uint32_t task_create(const char* pid, void (*entry_point)(void));
void task_yield(void);
void task_wait(uint32_t pid);
void task_exit(void);

#endif