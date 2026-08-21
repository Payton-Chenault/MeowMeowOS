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

extern void switch_to_task(uint32_t *old_esp, uint32_t new_esp,
                           uint32_t new_cr3);
extern void switch_to_user_task(uint32_t *old_esp, uint32_t new_esp,
                                uint32_t new_cr3);

static task_t *current_task = NULL;
static task_t *task_list = NULL;
static uint32_t next_pid = 1;
static spinlock_t task_lock = SPINLOCK_INIT;

static bool needs_cleanup = false;

static task_t *task_find_by_pid(uint32_t pid)
{
  task_t *task = task_list;
  while (task != NULL)
  {
    if (task->pid == pid)
    {
      return task;
    }
    task = task->next;
  }
  return NULL;
}

static bool task_add_child(task_t *parent, uint32_t child_pid)
{
  if (parent == NULL || parent->child_count >= MAX_CHILDREN)
  {
    return false;
  }

  for (uint32_t i = 0; i < parent->child_count; i++)
  {
    if (parent->child_pids[i] == child_pid)
    {
      return true;
    }
  }

  parent->child_pids[parent->child_count++] = child_pid;
  return true;
}

static bool task_can_reap(task_t *task)
{
  if (task == NULL || task->state != TASK_STATE_DEAD)
  {
    return false;
  }

  if (!task->is_zombie)
  {
    return true;
  }

  if (task->parent_pid == 0)
  {
    return true;
  }

  task_t *parent = task_find_by_pid(task->parent_pid);
  if (parent == NULL)
  {
    return true;
  }

  if (parent->state == TASK_STATE_DEAD)
  {
    return true;
  }

  if (parent->state == TASK_STATE_WAITING &&
      parent->waiting_on_pid == task->pid)
  {
    return true;
  }

  return false;
}

static void task_wake_expired_sleepers(void)
{
  if (task_list == NULL)
  {
    return;
  }

  uint32_t now = get_ticks();
  task_t *task = task_list;
  while (task != NULL)
  {
    if (task->state == TASK_STATE_SLEEPING && task->wake_tick != 0 &&
        now >= task->wake_tick)
    {
      uint32_t old_wake_tick = task->wake_tick;
      uint32_t elapsed_ticks = now - old_wake_tick + 1;
      task->state = TASK_STATE_READY;
      task->wake_tick = 0;
      log_debug(MODULE, "woke task %s (pid=%u) after %u ticks", task->name,
                task->pid, elapsed_ticks);
    }
    task = task->next;
  }
}

static task_t *task_find_next_ready(task_t *start)
{
  if (task_list == NULL)
    return NULL;

  task_t *candidate = (start != NULL) ? start->next : task_list;
  if (candidate == NULL)
    candidate = task_list;

  for (uint32_t tries = 0; tries < next_pid; tries++)
  {
    if (candidate == NULL)
      candidate = task_list;

    if (candidate == start)
      break;

    if (candidate->state == TASK_STATE_READY ||
        candidate->state == TASK_STATE_RUNNING)
    {
      return candidate;
    }

    candidate = candidate->next;
  }

  if (start != NULL &&
      (start->state == TASK_STATE_READY || start->state == TASK_STATE_RUNNING))
  {
    return start;
  }

  return task_list;
}

static void idle_task_function(void)
{
  while (1)
  {
    if (needs_cleanup)
    {
      spinlock_acquire_irq(&task_lock);

      task_t *prev = NULL;
      task_t *current = task_list;

      while (current != NULL)
      {
        if (current->state == TASK_STATE_DEAD && task_can_reap(current))
        {
          if (prev != NULL)
          {
            prev->next = current->next;
          }
          else
          {
            task_list = current->next;
          }

          task_t *task_to_delete = current;
          current = current->next;

          kmem_free((void *)task_to_delete->stack_base);
          kmem_free((void *)task_to_delete);
        }
        else
        {
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

void task_initialize()
{
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
  root_task->quantum = TASK_QUANTUM_DEFAULT;
  root_task->slice_remaining = TASK_QUANTUM_DEFAULT;
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

  task_create("idle", idle_task_function, (uint32_t)vmm_get_directory());

  log_info(MODULE, "Initialized");
}

uint32_t task_create(const char *name, void (*entry_point)(void),
                     uint32_t page_directory)
{
  task_t *new_task = (task_t *)kmem_zalloc(sizeof(task_t));

  uint32_t *stack = (uint32_t *)kmem_zalloc(16384);
  uint32_t *esp = (uint32_t *)((uint32_t)stack + 16384);

  // switch_to_task expects the stack to contain, from top to bottom:
  // edi, esi, ebx, ebp, return address
  esp -= 5;                       // make room for 5 dwords
  esp[0] = 0;                     // edi
  esp[1] = 0;                     // esi
  esp[2] = 0;                     // ebx
  esp[3] = 0;                     // ebp
  esp[4] = (uint32_t)entry_point; // return address

  new_task->stack_base = (uint32_t)stack;
  new_task->kernel_stack_top = (uint32_t)stack + 16384;
  new_task->is_user = false;
  new_task->uid = 0;
  new_task->gid = 0;
  new_task->euid = 0;
  new_task->egid = 0;
  new_task->caps = TASK_CAPS_FULL;
  new_task->umask = 0022;
  new_task->quantum = TASK_QUANTUM_DEFAULT;
  new_task->slice_remaining = TASK_QUANTUM_DEFAULT;
  new_task->wake_tick = 0;
  new_task->parent_pid = current_task ? current_task->pid : 0;
  new_task->exit_status = 0;
  new_task->child_count = 0;
  new_task->cpu_time_ticks = 0;
  new_task->yield_requested = false;
  new_task->is_zombie = false;

  new_task->fd_table[0].in_use = true;
  new_task->fd_table[0].node = vfs_find("stdin");
  new_task->fd_table[1].in_use = true;
  new_task->fd_table[1].node = vfs_find("stdout");
  new_task->fd_table[2].in_use = true;
  new_task->fd_table[2].node = vfs_find("stdout");

  for (int i = 3; i < MAX_OPEN_FILES; i++)
  {
    new_task->fd_table[i].in_use = false;
  }

  new_task->pid = next_pid++;
  if (current_task != NULL)
  {
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

  log_debug(MODULE, "Created kernel task %s: (PID: %d)", name, new_task->pid);
  return new_task->pid;
}

uint32_t task_create_user(const char *name, uint32_t entry_point,
                          uint32_t page_directory)
{
  task_t *new_task = (task_t *)kmem_zalloc(sizeof(task_t));

  uint32_t *kstack = (uint32_t *)kmem_zalloc(16384);
  uint32_t stack_top = (uint32_t)kstack + 16384;
  stack_top &= ~0xF; // 16-byte align

  new_task->stack_base = (uint32_t)kstack;
  new_task->kernel_stack_top = stack_top;
  new_task->is_user = true;
  new_task->uid = 0;
  new_task->gid = 0;
  new_task->euid = 0;
  new_task->egid = 0;
  new_task->caps = TASK_CAPS_FULL;
  new_task->umask = 0022;
  new_task->quantum = TASK_QUANTUM_DEFAULT;
  new_task->slice_remaining = TASK_QUANTUM_DEFAULT;
  new_task->wake_tick = 0;
  new_task->parent_pid = current_task ? current_task->pid : 0;
  new_task->exit_status = 0;
  new_task->child_count = 0;
  new_task->cpu_time_ticks = 0;
  new_task->yield_requested = false;
  new_task->is_zombie = false;

  uint32_t *esp = (uint32_t *)stack_top;

  // IRET frame expected by switch_to_user_task:
  // [EIP, CS, EFLAGS, ESP, SS]
  *(--esp) = 0x23;                   // SS
  *(--esp) = 0xBFFFF000 + 4096 - 16; // ESP
  *(--esp) = 0x202;                  // EFLAGS
  *(--esp) = 0x1B;                   // CS
  *(--esp) = entry_point;            // EIP

  new_task->esp = (uint32_t)esp;

  new_task->fd_table[0].in_use = true;
  new_task->fd_table[0].node = vfs_find("stdin");
  new_task->fd_table[1].in_use = true;
  new_task->fd_table[1].node = vfs_find("stdout");
  new_task->fd_table[2].in_use = true;
  new_task->fd_table[2].node = vfs_find("stdout");
  for (int i = 3; i < MAX_OPEN_FILES; i++)
  {
    new_task->fd_table[i].in_use = false;
  }

  const char *base_name = name;
  const char *slash = strrchr(name, '/');
  if (slash != NULL)
  {
    base_name = slash + 1;
  }

  strncpy(new_task->name, base_name, sizeof(new_task->name) - 1);
  new_task->name[sizeof(new_task->name) - 1] = '\0';

  strncpy(new_task->exec_path, name, sizeof(new_task->exec_path) - 1);
  new_task->exec_path[sizeof(new_task->exec_path) - 1] = '\0';

  new_task->pid = next_pid++;
  if (current_task != NULL)
  {
    task_add_child(current_task, new_task->pid);
  }
  new_task->state = TASK_STATE_READY;
  new_task->page_directory = page_directory;
  new_task->next_user_vaddr = 0x10000000u;

  spinlock_acquire(&task_lock);
  new_task->next = task_list;
  task_list = new_task;
  spinlock_release(&task_lock);

  log_debug(MODULE, "Created user task %s (full path: %s) (PID: %d), esp=%x",
            new_task->name, new_task->exec_path, new_task->pid, new_task->esp);
  return new_task->pid;
}

void task_schedule_tick(void)
{
  task_wake_expired_sleepers();

  if (!current_task || current_task->state == TASK_STATE_DEAD ||
      current_task->state == TASK_STATE_BLOCKED ||
      current_task->state == TASK_STATE_SLEEPING)
  {
    return;
  }

  if (current_task->yield_requested)
  {
    current_task->yield_requested = false;
    current_task->cpu_time_ticks++;
    task_yield();
    return;
  }

  current_task->cpu_time_ticks++;

  if (current_task->is_user)
  {
    current_task->slice_remaining = current_task->quantum > 0
                                        ? current_task->quantum
                                        : TASK_QUANTUM_DEFAULT;
    return;
  }

  if (current_task->slice_remaining > 0)
  {
    current_task->slice_remaining--;
  }

  if (current_task->slice_remaining == 0)
  {
    current_task->state = TASK_STATE_READY;
    current_task->slice_remaining = current_task->quantum > 0
                                        ? current_task->quantum
                                        : TASK_QUANTUM_DEFAULT;
    task_yield();
  }
}

void task_sleep(uint32_t ticks)
{
  if (ticks == 0 || current_task == NULL)
  {
    return;
  }

  uint32_t flags = spinlock_acquire_irq_save(&task_lock);
  current_task->state = TASK_STATE_SLEEPING;
  current_task->wake_tick = get_ticks() + ticks;
  current_task->slice_remaining = 0;

  // [FIX 4] Do not restore IRQs before yielding to prevent the timer
  // from firing before the task has safely switched out.
  spinlock_release(&task_lock);

  task_yield();

  // Re-enable interrupts if they were enabled before task_sleep was called
  if (flags & 0x200)
  { // Check if IF flag was set in EFLAGS
    enable_interrupts();
  }
}

void task_wake(uint32_t pid)
{
  if (task_list == NULL)
  {
    return;
  }

  uint32_t flags = spinlock_acquire_irq_save(&task_lock);
  task_t *task = task_list;
  while (task != NULL)
  {
    if (task->pid == pid && task->state == TASK_STATE_SLEEPING)
    {
      task->state = TASK_STATE_READY;
      task->wake_tick = 0;
      task->slice_remaining = task->quantum > 0 ? task->quantum
                                                : TASK_QUANTUM_DEFAULT;
      break;
    }
    task = task->next;
  }
  spinlock_release_irq_restore(&task_lock, flags);
}

void task_request_yield(void)
{
  if (!current_task)
  {
    return;
  }

  uint32_t flags = spinlock_acquire_irq_save(&task_lock);
  current_task->yield_requested = true;
  spinlock_release_irq_restore(&task_lock, flags);
}

void task_yield()
{
  uint32_t flags = spinlock_acquire_irq_save(&task_lock);

  if (!current_task)
  {
    spinlock_release_irq_restore(&task_lock, flags);
    return;
  }

  bool allow_user_switch = current_task->yield_requested ||
                           current_task->state == TASK_STATE_DEAD;

  // User tasks are cooperative and must only switch when they explicitly request
  // a yield or when they are exiting. This keeps the switch at a safe
  // scheduler boundary instead of from the middle of an active interrupt frame.
  if (current_task->is_user && !allow_user_switch &&
      current_task->state != TASK_STATE_DEAD)
  {
    spinlock_release_irq_restore(&task_lock, flags);
    return;
  }

  if (current_task->yield_requested)
  {
    current_task->yield_requested = false;
  }

  task_t *next = task_find_next_ready(current_task);

  if (next == NULL || next == current_task)
  {
    spinlock_release_irq_restore(&task_lock, flags);
    return;
  }

  if (current_task->state == TASK_STATE_RUNNING)
  {
    current_task->state = TASK_STATE_READY;
  }

  task_t *old = current_task;
  current_task = next;
  current_task->state = TASK_STATE_RUNNING;
  current_task->slice_remaining =
      current_task->quantum > 0 ? current_task->quantum : TASK_QUANTUM_DEFAULT;

  tss_set_kernel_stack(current_task->kernel_stack_top);

  spinlock_release_irq_restore(&task_lock, flags);

  if (next->is_user)
  {
    switch_to_user_task(&old->esp, next->esp, next->page_directory);
  }
  else
  {
    switch_to_task(&old->esp, next->esp, next->page_directory);
  }
}

void task_exit_with_status(int status)
{
  uint32_t flags = spinlock_acquire_irq_save(&task_lock);
  if (current_task == NULL)
  {
    spinlock_release_irq_restore(&task_lock, flags);
    return;
  }

  current_task->exit_status = (uint32_t)status;
  current_task->state = TASK_STATE_DEAD;
  current_task->is_zombie = true;

  task_t *temp = task_list;
  while (temp)
  {
    if (temp->state == TASK_STATE_WAITING &&
        temp->waiting_on_pid == current_task->pid)
    {
      temp->state = TASK_STATE_READY;
      temp->waiting_on_pid = 0;
      temp->exit_status = current_task->exit_status;
    }
    temp = temp->next;
  }

  for (int i = 3; i < MAX_OPEN_FILES; i++)
  {
    if (current_task->fd_table[i].in_use)
    {
      vfs_node_t *node = current_task->fd_table[i].node;
      if (node != NULL && node->type == VFS_FILE)
      {
        kmem_free(node);
      }
      current_task->fd_table[i].in_use = false;
    }
  }

  needs_cleanup = true;
  spinlock_release_irq_restore(&task_lock, flags);

  task_yield();
}

void task_exit(void) { task_exit_with_status(0); }

void task_wait(uint32_t pid)
{
  uint32_t flags = spinlock_acquire_irq_save(&task_lock);

  task_t *target = task_find_by_pid(pid);
  if (target == NULL)
  {
    spinlock_release_irq_restore(&task_lock, flags);
    return;
  }

  if (target->state == TASK_STATE_DEAD)
  {
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

uint32_t task_get_exit_status(uint32_t pid)
{
  task_t *task = task_find_by_pid(pid);
  return task ? task->exit_status : 0;
}

uint32_t task_get_cpu_time(uint32_t pid)
{
  task_t *task = task_find_by_pid(pid);
  return task ? task->cpu_time_ticks : 0;
}

task_t *task_get_current(void) { return current_task; }