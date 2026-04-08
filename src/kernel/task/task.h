#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define TASK_STATE_READY 0
#define TASK_STATE_RUNNING 1

typedef struct {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t eip, cs, eflags;
} cpu_state_t;

typedef struct task {
    uint32_t pid;
    char name[32];
    uint32_t esp;
    uint32_t page_directory;
    uint8_t state;
    struct task* next;
} task_t;

void task_initialize(void);
void task_create(const char* pid, void (*entry_point)(void));
void task_yield(void);

#endif