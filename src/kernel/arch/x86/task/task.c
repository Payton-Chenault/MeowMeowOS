#include "task.h"
#include "../../../kernel_services/kernel_services.h"
#include "../../../lib/string/string.h"
#include "../../../mem/virtual_memory_manager/vmm.h"
#include "../../../utils/logging/logger.h"
#include "../pit/pit.h"
#include "../global_descriptor_table/gdt.h"
#include "../interrupt_descriptor_table/idt.h"
#include "../sync/spinlock.h"
#include <stdint.h>

#define MODULE "TASK"

extern void switch_to_task(uint32_t *old_esp, uint32_t new_esp, uint32_t new_cr3);
extern void switch_to_user_task(uint32_t *old_esp, uint32_t new_esp, uint32_t new_cr3);

static task_t *current_task = NULL;
static task_t *task_list = NULL;
static uint32_t next_pid = 1;
static spinlock_t task_lock = SPINLOCK_INIT;
static bool needs_cleanup = false;

static inline uint32_t task_calculate_quantum(uint8_t priority) {
  switch (priority) {
    case PRIORITY_IDLE:     return 2;
    case PRIORITY_LOW:      return 5;
    case PRIORITY_NORMAL:   return 10;
    case PRIORITY_HIGH:     return 20;
    case PRIORITY_REALTIME: return 30;
    default:                return 10;
  }
}

task_t *task_get_by_pid(uint32_t pid) {
  task_t *task = task_list;
  while (task != NULL) {
    if (task->pid == pid) {
      return task;
    }
    task = task->next;
  }
  return NULL;
}

static bool task_add_child(task_t *parent, uint32_t child_pid) {
  if (parent == NULL || parent->child_count >= MAX_CHILDREN) {
    return false;
  }
  for (uint32_t i = 0; i < parent->child_count; i++) {
    if (parent->child_pids[i] == child_pid) {
      return true;
    }
  }
  parent->child_pids[parent->child_count++] = child_pid;
  return true;
}

static bool task_can_reap(task_t *task) {
  if (task == NULL || task->state != TASK_STATE_DEAD) return false;
  if (!task->is_zombie || task->parent_pid == 0) return true;

  task_t *parent = task_get_by_pid(task->parent_pid);
  if (parent == NULL || parent->state == TASK_STATE_DEAD) return true;
  if (parent->state == TASK_STATE_WAITING && parent->waiting_on_pid == task->pid) {
    return true;
  }
  return false;
}

static void task_wake_expired_sleepers(void) {
  if (task_list == NULL) return;

  uint32_t now = get_ticks();
  task_t *task = task_list;
  while (task != NULL) {
    if (task->state == TASK_STATE_SLEEPING && task->wake_tick != 0 && now >= task->wake_tick) {
      task->state = TASK_STATE_READY;
      task->wake_tick = 0;
      task->wait_ticks = 0;
    }
    task = task->next;
  }
}

/* Fair Priority Finder with Aging / Anti-Starvation */
static task_t *task_find_next_ready(task_t *current) {
  if (task_list == NULL) return NULL;

  task_t *best_task = NULL;
  uint8_t highest_prio = 0;
  uint32_t longest_wait = 0;

  task_t *t = task_list;
  while (t != NULL) {
    if (t->state == TASK_STATE_READY || (t == current && t->state == TASK_STATE_RUNNING)) {
      /* Aging Algorithm: Boost priority if starving in READY queue */
      if (t != current && t->state == TASK_STATE_READY) {
        t->wait_ticks++;
        if (t->wait_ticks >= STARVATION_THRESHOLD && t->dynamic_priority < PRIORITY_MAX) {
          t->dynamic_priority++;
          t->wait_ticks = 0;
          log_trace(MODULE, "Aging: Boosted task %s (PID: %u) to priority %u",
                    t->name, t->pid, t->dynamic_priority);
        }
      }

      /* Selection Criteria: Highest Dynamic Priority -> Longest Wait Time */
      if (best_task == NULL ||
          t->dynamic_priority > highest_prio ||
          (t->dynamic_priority == highest_prio && t->wait_ticks > longest_wait)) {
        best_task = t;
        highest_prio = t->dynamic_priority;
        longest_wait = t->wait_ticks;
      }
    }
    t = t->next;
  }

  return best_task;
}

static void idle_task_function(void) {
  while (1) {
    if (needs_cleanup) {
      spinlock_acquire_irq(&task_lock);
      task_t *prev = NULL;
      task_t *curr = task_list;

      while (curr != NULL) {
        if (curr->state == TASK_STATE_DEAD && task_can_reap(curr)) {
          if (prev != NULL) {
            prev->next = curr->next;
          } else {
            task_list = curr->next;
          }
          task_t *to_delete = curr;
          curr = curr->next;
          kmem_free((void *)to_delete->stack_base);
          kmem_free((void *)to_delete);
        } else {
          prev = curr;
          curr = curr->next;
        }
      }
      needs_cleanup = false;
      spinlock_release_irq(&task_lock);
    }
    wait_for_interrupt();
  }
}

void task_initialize() {
  task_t *root_task = (task_t *)kmem_zalloc(sizeof(task_t));

  root_task->pid = next_pid++;
  strcpy(root_task->name, "kernel_shell");
  root_task->state = TASK_STATE_RUNNING;
  root_task->page_directory = (uint32_t)vmm_get_directory();
  root_task->next_user_vaddr = 0x10000000u;
  root_task->is_user = false;
  root_task->uid = 0;
  root_task->gid = 0;
  root_task->euid = 0;
  root_task->egid = 0;
  root_task->caps = TASK_CAPS_FULL;
  root_task->umask = 0022;

  root_task->base_priority = PRIORITY_NORMAL;
  root_task->dynamic_priority = PRIORITY_NORMAL;
  root_task->wait_ticks = 0;
  root_task->quantum = task_calculate_quantum(PRIORITY_NORMAL);
  root_task->slice_remaining = root_task->quantum;

  root_task->wake_tick = 0;
  root_task->parent_pid = 0;
  root_task->exit_status = 0;
  root_task->child_count = 0;
  root_task->cpu_time_ticks = 0;
  root_task->yield_requested = false;
  root_task->is_zombie = false;

  root_task->fd_table[0].node = vfs_find("stdin");
  root_task->fd_table[1].node = vfs_find("stdout");
  root_task->fd_table[2].node = vfs_find("stdout");

  uint32_t *root_stack = (uint32_t *)kmem_zalloc(16384);
  root_task->stack_base = (uint32_t)root_stack;
  root_task->kernel_stack_top = (uint32_t)root_stack + 16384;
  root_task->esp = root_task->kernel_stack_top;

  tss_set_kernel_stack(root_task->kernel_stack_top);

  task_list = root_task;
  current_task = root_task;

  /* Initialize the Idle Task with Lowest Priority */
  uint32_t idle_pid = task_create("idle", idle_task_function, (uint32_t)vmm_get_directory());
  task_set_priority(idle_pid, PRIORITY_IDLE);

  log_info(MODULE, "Scheduler Initialized with Priority & Fair Aging");
}

uint32_t task_create(const char *name, void (*entry_point)(void), uint32_t page_directory) {
  task_t *new_task = (task_t *)kmem_zalloc(sizeof(task_t));

  uint32_t *stack = (uint32_t *)kmem_zalloc(16384);
  uint32_t *esp = (uint32_t *)((uint32_t)stack + 16384);

  esp -= 5;
  esp[0] = 0;
  esp[1] = 0;
  esp[2] = 0;
  esp[3] = 0;
  esp[4] = (uint32_t)entry_point;

  new_task->stack_base = (uint32_t)stack;
  new_task->kernel_stack_top = (uint32_t)stack + 16384;
  new_task->is_user = false;
  new_task->uid = 0;
  new_task->gid = 0;
  new_task->euid = 0;
  new_task->egid = 0;
  new_task->caps = TASK_CAPS_FULL;
  new_task->umask = 0022;

  new_task->base_priority = PRIORITY_NORMAL;
  new_task->dynamic_priority = PRIORITY_NORMAL;
  new_task->wait_ticks = 0;
  new_task->quantum = task_calculate_quantum(PRIORITY_NORMAL);
  new_task->slice_remaining = new_task->quantum;

  new_task->wake_tick = 0;
  new_task->parent_pid = current_task ? current_task->pid : 0;
  new_task->exit_status = 0;
  new_task->child_count = 0;
  new_task->cpu_time_ticks = 0;
  new_task->yield_requested = false;
  new_task->tls_ptr = 0;

  new_task->fd_table[0].in_use = true;
  new_task->fd_table[0].node = vfs_find("stdin");
  new_task->fd_table[1].in_use = true;
  new_task->fd_table[1].node = vfs_find("stdout");
  new_task->fd_table[2].in_use = true;
  new_task->fd_table[2].node = vfs_find("stdout");
  for (int i = 3; i < MAX_OPEN_FILES; i++) new_task->fd_table[i].in_use = false;

  new_task->pid = next_pid++;
  if (current_task != NULL) {
    task_add_child(current_task, new_task->pid);
  }
  strcpy(new_task->name, name);
  new_task->esp = (uint32_t)esp;
  new_task->state = TASK_STATE_READY;
  new_task->exec_path[0] = '\0';
  new_task->page_directory = page_directory;
  new_task->next_user_vaddr = 0x10000000u;

  spinlock_acquire(&task_lock);
  new_task->next = task_list;
  task_list = new_task;
  spinlock_release(&task_lock);

  log_debug(MODULE, "Created kernel task %s: (PID: %d, Priority: %u)", 
            name, new_task->pid, new_task->base_priority);
  return new_task->pid;
}

uint32_t task_create_user(const char *name, uint32_t entry_point, uint32_t page_directory) {
  task_t *new_task = (task_t *)kmem_zalloc(sizeof(task_t));

  uint32_t *kstack = (uint32_t *)kmem_zalloc(16384);
  uint32_t stack_top = (uint32_t)kstack + 16384;
  stack_top &= ~0xF;

  new_task->stack_base = (uint32_t)kstack;
  new_task->kernel_stack_top = stack_top;
  new_task->is_user = true;
  new_task->uid = 0;
  new_task->gid = 0;
  new_task->euid = 0;
  new_task->egid = 0;
  new_task->caps = TASK_CAPS_FULL;
  new_task->umask = 0022;

  new_task->base_priority = PRIORITY_NORMAL;
  new_task->dynamic_priority = PRIORITY_NORMAL;
  new_task->wait_ticks = 0;
  new_task->quantum = task_calculate_quantum(PRIORITY_NORMAL);
  new_task->slice_remaining = new_task->quantum;

  new_task->wake_tick = 0;
  new_task->parent_pid = current_task ? current_task->pid : 0;
  new_task->exit_status = 0;
  new_task->child_count = 0;
  new_task->cpu_time_ticks = 0;
  new_task->yield_requested = false;
  new_task->is_zombie = false;
  new_task->tls_ptr = 0;

  new_task->heap_start = 0;
  new_task->heap_break = 0;

  uint32_t *esp = (uint32_t *)stack_top;
  *(--esp) = 0x23;
  *(--esp) = 0xBFFFF000 + 4096 - 16;
  *(--esp) = 0x202;
  *(--esp) = 0x1B;
  *(--esp) = entry_point;

  new_task->esp = (uint32_t)esp;

  new_task->fd_table[0].in_use = true;
  new_task->fd_table[0].node = vfs_find("stdin");
  new_task->fd_table[1].in_use = true;
  new_task->fd_table[1].node = vfs_find("stdout");
  new_task->fd_table[2].in_use = true;
  new_task->fd_table[2].node = vfs_find("stdout");
  for (int i = 3; i < MAX_OPEN_FILES; i++) new_task->fd_table[i].in_use = false;

  const char *base_name = name;
  const char *slash = strrchr(name, '/');
  if (slash != NULL) base_name = slash + 1;

  strncpy(new_task->name, base_name, sizeof(new_task->name) - 1);
  new_task->name[sizeof(new_task->name) - 1] = '\0';

  strncpy(new_task->exec_path, name, sizeof(new_task->exec_path) - 1);
  new_task->exec_path[sizeof(new_task->exec_path) - 1] = '\0';

  new_task->pid = next_pid++;
  if (current_task != NULL) {
    task_add_child(current_task, new_task->pid);
  }
  new_task->state = TASK_STATE_READY;
  new_task->page_directory = page_directory;
  new_task->next_user_vaddr = 0x10000000u;

  spinlock_acquire(&task_lock);
  new_task->next = task_list;
  task_list = new_task;
  spinlock_release(&task_lock);

  log_debug(MODULE, "Created user task %s (PID: %d, Priority: %u)",
            new_task->name, new_task->pid, new_task->base_priority);
  return new_task->pid;
}

void task_schedule_tick(void) {
  task_wake_expired_sleepers();

  if (!current_task) return;

  if (current_task->state == TASK_STATE_DEAD) {
    task_yield();
    return;
  }

  /* Blocked/Waiting/Sleeping tasks do not consume timeslices */
  if (current_task->state == TASK_STATE_BLOCKED ||
      current_task->state == TASK_STATE_SLEEPING ||
      current_task->state == TASK_STATE_WAITING) {
    return;
  }

  current_task->cpu_time_ticks++;

  if (current_task->yield_requested) {
    current_task->yield_requested = false;
    task_yield();
    return;
  }

  /* Decrement remaining timeslice for both Kernel and User tasks */
  if (current_task->slice_remaining > 0) {
    current_task->slice_remaining--;
  }

  /* Timeslice Expired: Preempt and schedule next highest priority task */
  if (current_task->slice_remaining == 0) {
    if (current_task->state == TASK_STATE_RUNNING) {
      current_task->state = TASK_STATE_READY;
    }
    task_yield();
  }
}

void task_yield() {
  uint32_t flags = spinlock_acquire_irq_save(&task_lock);

  if (!current_task) {
    spinlock_release_irq_restore(&task_lock, flags);
    return;
  }

  task_t *next = task_find_next_ready(current_task);

  if (next == NULL || next == current_task) {
    if (current_task->state == TASK_STATE_RUNNING) {
      current_task->slice_remaining = task_calculate_quantum(current_task->base_priority);
    }
    spinlock_release_irq_restore(&task_lock, flags);
    return;
  }

  if (current_task->state == TASK_STATE_RUNNING) {
    current_task->state = TASK_STATE_READY;
  }

  task_t *old = current_task;
  current_task = next;
  current_task->state = TASK_STATE_RUNNING;
  
  /* Reset dynamic priority back to base priority upon getting CPU time */
  current_task->dynamic_priority = current_task->base_priority;
  current_task->wait_ticks = 0;
  current_task->quantum = task_calculate_quantum(current_task->base_priority);
  current_task->slice_remaining = current_task->quantum;

  tss_set_kernel_stack(current_task->kernel_stack_top);
  spinlock_release_irq_restore(&task_lock, flags);

  if (next->is_user) {
    switch_to_user_task(&old->esp, next->esp, next->page_directory);
  } else {
    switch_to_task(&old->esp, next->esp, next->page_directory);
  }
}

void task_block(void) {
  uint32_t flags = spinlock_acquire_irq_save(&task_lock);
  if (current_task != NULL) {
    current_task->state = TASK_STATE_BLOCKED;
    current_task->slice_remaining = 0;
  }
  spinlock_release_irq_restore(&task_lock, flags);
  task_yield();
}

void task_unblock(uint32_t pid) {
  uint32_t flags = spinlock_acquire_irq_save(&task_lock);
  task_t *task = task_get_by_pid(pid);
  if (task != NULL && task->state == TASK_STATE_BLOCKED) {
    task->state = TASK_STATE_READY;
    task->wait_ticks = 0;
    task->slice_remaining = task_calculate_quantum(task->base_priority);
  }
  spinlock_release_irq_restore(&task_lock, flags);
}

void task_set_priority(uint32_t pid, uint8_t priority) {
  if (priority > PRIORITY_MAX) priority = PRIORITY_MAX;
  uint32_t flags = spinlock_acquire_irq_save(&task_lock);
  task_t *task = task_get_by_pid(pid);
  if (task != NULL) {
    task->base_priority = priority;
    task->dynamic_priority = priority;
    task->quantum = task_calculate_quantum(priority);
  }
  spinlock_release_irq_restore(&task_lock, flags);
}

uint8_t task_get_priority(uint32_t pid) {
  task_t *task = task_get_by_pid(pid);
  return task ? task->base_priority : PRIORITY_NORMAL;
}

void task_sleep(uint32_t ticks) {
  if (ticks == 0 || current_task == NULL) return;

  uint32_t flags = spinlock_acquire_irq_save(&task_lock);
  current_task->state = TASK_STATE_SLEEPING;
  current_task->wake_tick = get_ticks() + ticks;
  current_task->slice_remaining = 0;
  spinlock_release(&task_lock);

  task_yield();

  if (flags & 0x200) enable_interrupts();
}

void task_wake(uint32_t pid) {
  if (task_list == NULL) return;
  uint32_t flags = spinlock_acquire_irq_save(&task_lock);
  task_t *task = task_list;
  while (task != NULL) {
    if (task->pid == pid && task->state == TASK_STATE_SLEEPING) {
      task->state = TASK_STATE_READY;
      task->wake_tick = 0;
      task->wait_ticks = 0;
      task->slice_remaining = task_calculate_quantum(task->base_priority);
      break;
    }
    task = task->next;
  }
  spinlock_release_irq_restore(&task_lock, flags);
}

void task_request_yield(void) {
  if (!current_task) return;
  uint32_t flags = spinlock_acquire_irq_save(&task_lock);
  current_task->yield_requested = true;
  spinlock_release_irq_restore(&task_lock, flags);
}

void task_exit_with_status(int status) {
  uint32_t flags = spinlock_acquire_irq_save(&task_lock);
  if (current_task == NULL) {
    spinlock_release_irq_restore(&task_lock, flags);
    return;
  }

  current_task->exit_status = (uint32_t)status;
  current_task->state = TASK_STATE_DEAD;
  current_task->is_zombie = true;

  task_t *temp = task_list;
  while (temp) {
    if (temp->state == TASK_STATE_WAITING && temp->waiting_on_pid == current_task->pid) {
      temp->state = TASK_STATE_READY;
      temp->waiting_on_pid = 0;
      temp->exit_status = current_task->exit_status;
    }
    temp = temp->next;
  }

  for (int i = 3; i < MAX_OPEN_FILES; i++) {
    if (current_task->fd_table[i].in_use) {
      vfs_node_t *node = current_task->fd_table[i].node;
      if (node != NULL && node->type == VFS_FILE) kmem_free(node);
      current_task->fd_table[i].in_use = false;
    }
  }

  needs_cleanup = true;
  spinlock_release_irq_restore(&task_lock, flags);

  enable_interrupts();
  while (1) {
    task_yield();
    __asm__ volatile("pause");
  }
}

void task_exit(void) { task_exit_with_status(0); }

void task_wait(uint32_t pid) {
  uint32_t flags = spinlock_acquire_irq_save(&task_lock);
  task_t *target = task_get_by_pid(pid);
  if (target == NULL) {
    spinlock_release_irq_restore(&task_lock, flags);
    return;
  }

  if (target->state == TASK_STATE_DEAD) {
    current_task->exit_status = target->exit_status;
    current_task->waiting_on_pid = 0;
    spinlock_release_irq_restore(&task_lock, flags);
    return;
  }

  current_task->state = TASK_STATE_WAITING;
  current_task->waiting_on_pid = pid;
  current_task->wake_tick = 0;
  spinlock_release_irq_restore(&task_lock, flags);
  
  task_yield();
  enable_interrupts();
}

uint32_t task_get_exit_status(uint32_t pid) {
  task_t *task = task_get_by_pid(pid);
  return task ? task->exit_status : 0;
}

uint32_t task_get_cpu_time(uint32_t pid) {
  task_t *task = task_get_by_pid(pid);
  return task ? task->cpu_time_ticks : 0;
}

task_t *task_get_current(void) { return current_task; }

uint32_t task_get_process_info(sys_process_info_t *buffer, uint32_t max_entries) {
  uint32_t count = 0;
  uint32_t flags = spinlock_acquire_irq_save(&task_lock);
  
  task_t *current = task_list;
  while (current != NULL && count < max_entries) {
    buffer[count].pid = current->pid;
    buffer[count].parent_pid = current->parent_pid;
    buffer[count].state = current->state;
    buffer[count].cpu_ticks = current->cpu_time_ticks;
    buffer[count].base_priority = current->base_priority;
    buffer[count].dynamic_priority = current->dynamic_priority;
    
    strncpy(buffer[count].name, current->name, sizeof(buffer[count].name) - 1);
    buffer[count].name[sizeof(buffer[count].name) - 1] = '\0';
    
    count++;
    current = current->next;
  }
  
  spinlock_release_irq_restore(&task_lock, flags);
  return count;
}