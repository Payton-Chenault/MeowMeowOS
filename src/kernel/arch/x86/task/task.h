#ifndef TASK_H
#define TASK_H

#include "../../../fs/vfs/vfs.h"
#include "../../../syscall/syscall.h"
#include <stdbool.h>
#include <stdint.h>

#define TASK_STATE_READY    0
#define TASK_STATE_RUNNING  1
#define TASK_STATE_WAITING  2
#define TASK_STATE_BLOCKED  3
#define TASK_STATE_SLEEPING 4
#define TASK_STATE_DEAD     5

/* Priority Levels */
#define PRIORITY_IDLE       0
#define PRIORITY_LOW        1
#define PRIORITY_NORMAL     2
#define PRIORITY_HIGH       3
#define PRIORITY_REALTIME   4
#define PRIORITY_MAX        4

/* Starvation Protection: Ticks waiting in READY queue before priority boost */
#define STARVATION_THRESHOLD 50
#define TASK_QUANTUM_DEFAULT 10

#define MAX_OPEN_FILES 16
#define MAX_CHILDREN   16

#define CAP_DEV_OPEN      (1u << 0)
#define CAP_FS_WRITE      (1u << 1)
#define CAP_FS_FORMAT     (1u << 2)
#define CAP_SYS_ADMIN     (1u << 3)
#define TASK_CAPS_FULL    0xFFFFFFFFu

typedef struct {
  bool in_use;
  char filename[256];
  uint32_t current_offset;
  uint32_t file_size;
  struct vfs_node *node;
} file_descriptor_t;

typedef struct {
  uint32_t es, ds;
  uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
  uint32_t eip, cs, eflags;
} cpu_state_t;

typedef struct task {
  uint32_t pid;
  char name[32];
  char exec_path[256];
  uint32_t esp;
  uint32_t page_directory;
  uint32_t next_user_vaddr;
  uint32_t uid;
  uint32_t gid;
  uint32_t euid;
  uint32_t egid;
  uint32_t caps;
  uint16_t umask;
  uint32_t stack_base;
  uint32_t kernel_stack_top;
  uint8_t state;
  uint8_t base_priority;
  uint8_t dynamic_priority;
  uint32_t wait_ticks;
  uint32_t quantum;
  uint32_t slice_remaining;
  uint32_t cpu_time_ticks;
  uint32_t waiting_on_pid;
  uint32_t wake_tick;
  uint32_t parent_pid;
  uint32_t exit_status;
  uint32_t child_pids[MAX_CHILDREN];
  uint32_t child_count;
  uint32_t user_rip;
  uint32_t is_user;
  uint32_t tls_ptr; 
  uint32_t heap_start;
  uint32_t heap_break;
  bool yield_requested;
  bool is_zombie;
  file_descriptor_t fd_table[MAX_OPEN_FILES];
  struct task *next;
} task_t;

void task_initialize(void);
uint32_t task_create_user(const char *name, uint32_t entry_point,
                          uint32_t page_directory);
uint32_t task_create(const char *name, void (*entry_point)(void),
                     uint32_t page_directory);

/* Scheduler & State APIs */
void task_request_yield(void);
void task_yield(void);
void task_schedule_tick(void);
void task_sleep(uint32_t ticks);
void task_wake(uint32_t pid);
void task_wait(uint32_t pid);
void task_block(void);
void task_unblock(uint32_t pid);
void task_set_priority(uint32_t pid, uint8_t priority);
uint8_t task_get_priority(uint32_t pid);

void task_exit(void);
void task_exit_with_status(int status);
uint32_t task_get_exit_status(uint32_t pid);
uint32_t task_get_cpu_time(uint32_t pid);
task_t *task_get_current(void);
task_t *task_get_by_pid(uint32_t pid);

uint32_t task_get_process_info(sys_process_info_t *buffer, uint32_t max_entries);
extern void enter_ring3(uint32_t entry_point, uint32_t user_stack);

#endif