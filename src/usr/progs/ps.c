#include "../libs/meow_libc.h"

#define MODULE "PS"
#define MAX_PROCS 32

DESCRIPTION("ps.elf: Report a snapshot of the current processes");

static void print_spaces(int count) {
    for (int i = 0; i < count; i++) {
        putchar(' ');
    }
}

static void print_num_padded(int num, int width) {
    char buf[16];
    itoa(num, buf, 10);
    printf("%s", buf);
    int len = strlen(buf);
    if (len < width) {
        print_spaces(width - len);
    }
}

static void print_str_padded(const char *str, int width) {
    if (!str) str = "";
    printf("%s", str);
    int len = strlen(str);
    if (len < width) {
        print_spaces(width - len);
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    log_trace(MODULE, "Fetching process snapshot from kernel");

    sys_process_info_t *procs = (sys_process_info_t *)malloc(MAX_PROCS * sizeof(sys_process_info_t));
    if (!procs) {
        printf("ps: out of memory allocating process table\n");
        return 1;
    }

    int count = sys_get_process_info(procs, MAX_PROCS);
    if (count < 0) {
        printf("ps: failed to get process info\n");
        free(procs);
        return 1;
    }

    log_trace(MODULE, "Retrieved %d active processes", count);

    printf("PID   PPID  PRIO  DYN   STATE   TICKS      COMMAND\n");
    printf("--------------------------------------------------\n");

    const char *state_names[] = {
        "READY", "RUN", "WAIT", "BLOCK", "SLEEP", "DEAD"
    };

    for (int i = 0; i < count; i++) {
        const char *state_str = "UNKNW";
        if (procs[i].state <= 5) {
            state_str = state_names[procs[i].state];
        }

        print_num_padded(procs[i].pid, 6);
        print_num_padded(procs[i].parent_pid, 6);
        print_num_padded(procs[i].base_priority, 6);
        print_num_padded(procs[i].dynamic_priority, 6);
        print_str_padded(state_str, 8);
        print_num_padded(procs[i].cpu_ticks, 11);
        printf("%s\n", procs[i].name);
    }

    printf("--------------------------------------------------\n");
    printf("Total tasks: %d\n", count);

    free(procs);
    log_trace(MODULE, "Process table display complete");
    return 0;
}