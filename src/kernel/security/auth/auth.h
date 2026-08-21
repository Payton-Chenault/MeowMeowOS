#ifndef AUTH_H
#define AUTH_H

#include "../../arch/x86/task/task.h"
#include "../../fs/vfs/vfs.h"
#include <stdbool.h>
#include <stdint.h>

bool task_has_cap(task_t *task, uint32_t cap);
bool vfs_check_access(vfs_node_t *node, task_t *task, uint8_t access);
bool task_is_root(void);

#endif