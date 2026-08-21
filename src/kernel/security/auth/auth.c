#include "auth.h"

#include <stddef.h>



bool task_has_cap(task_t *task, uint32_t cap) {
    if (task == NULL)
        return false;

    if (task->uid == 0 || task->euid == 0)
        return true;

    return (task->caps & cap) != 0;
}

bool vfs_check_access(vfs_node_t *node, task_t *task, uint8_t access) {
    if (node == NULL || task == NULL)
        return false;

    if (task->uid == 0 || task->euid == 0)
        return true;

    uint32_t shift = 0;

    if (task->euid == node->uid) {
        shift = 6;
    } else if (task->egid == node->gid) {
        shift = 3;
    } else {
        shift = 0;
    }

    uint16_t mode = node->mode;
    uint16_t mask = 0;

    if (access & VFS_ACCESS_READ)
        mask |= 4;
    if (access & VFS_ACCESS_WRITE)
        mask |= 2;
    if (access & VFS_ACCESS_EXEC)
        mask |= 1;

    return (mode & (mask << shift)) != 0;
}

bool task_is_root(void) {
    task_t *current = task_get_current();
    return current != NULL && (current->uid == 0 || current->euid == 0);
}