#include "auth.h"
#include "../../arch/x86/task/task.h"

bool task_is_root(void) {
    task_t* current = task_get_current();
    if (!current) {
        return false; 
    }
    return current->uid == 0;
}