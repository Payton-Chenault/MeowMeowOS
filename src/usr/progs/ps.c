#include "../libs/meow_libc.h"

DESCRIPTION("ps.elf: Report a snapshot of the current processes");

#define MAX_PROCS 64
static sys_process_info_t procs[MAX_PROCS];

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    int count = sys_get_process_info(procs, MAX_PROCS);

    if (count < 0) {
        printf("ps: failed to get process info\n");
        return 1;
    }

    printf("PID   PPID   PRIO  DYN   STATE      TICKS   CMD\n");
    
    const char* states[] = {
        "READY", "RUN  ", "WAIT ", "BLOCK", "SLEEP", "DEAD "
    };

    for (int i = 0; i < count; i++) {
        const char* state_str = "UNKNW";
        if (procs[i].state <= 5) {
            state_str = states[procs[i].state];
        }
        
        printf("%d     %d      %d     %d     %s      %d       %s\n", 
               procs[i].pid, 
               procs[i].parent_pid, 
               procs[i].base_priority,
               procs[i].dynamic_priority,
               state_str, 
               procs[i].cpu_ticks, 
               procs[i].name);
    }

    return 0;
}