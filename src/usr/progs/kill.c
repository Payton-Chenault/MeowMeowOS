#include "../libs/meow_libc.h"

DESCRIPTION("kill.elf: Send signals to processes");

typedef struct {
    const char *name;
    int signum;
} sig_map_t;

static const sig_map_t signal_table[] = {
    {"HUP",     SIGHUP},
    {"SIGHUP",  SIGHUP},
    {"INT",     SIGINT},
    {"SIGINT",  SIGINT},
    {"QUIT",    SIGQUIT},
    {"SIGQUIT", SIGQUIT},
    {"ILL",     SIGILL},
    {"SIGILL",  SIGILL},
    {"TRAP",    SIGTRAP},
    {"SIGTRAP", SIGTRAP},
    {"ABRT",    SIGABRT},
    {"SIGABRT", SIGABRT},
    {"FPE",     SIGFPE},
    {"SIGFPE",  SIGFPE},
    {"KILL",    SIGKILL},
    {"SIGKILL", SIGKILL},
    {"SEGV",    SIGSEGV},
    {"SIGSEGV", SIGSEGV},
    {"ALRM",    SIGALRM},
    {"SIGALRM", SIGALRM},
    {"TERM",    SIGTERM},
    {"SIGTERM", SIGTERM},
    {NULL, 0}
};

static void print_usage(void) {
    printf("Usage: kill [-s signal | -signal] <pid> ...\n");
    printf("       kill -l\n");
}

static void list_signals(void) {
    printf(" 1) SIGHUP       2) SIGINT       3) SIGQUIT      4) SIGILL\n");
    printf(" 5) SIGTRAP      6) SIGABRT      8) SIGFPE       9) SIGKILL\n");
    printf("11) SIGSEGV     14) SIGALRM     15) SIGTERM\n");
}

static int parse_signal(const char *str) {
    if (!str || str[0] == '\0') return -1;
    
    // Numeric signal specification (e.g. 9, 15)
    if (str[0] >= '0' && str[0] <= '9') {
        int sig = atoi(str);
        if (sig > 0 && sig < NSIG) return sig;
        return -1;
    }
    
    // Named signal specification (e.g. SIGKILL, KILL, INT)
    for (int i = 0; signal_table[i].name != NULL; i++) {
        if (strcasecmp(str, signal_table[i].name) == 0) {
            return signal_table[i].signum;
        }
    }
    return -1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "-l") == 0 || strcmp(argv[1], "--list") == 0) {
        list_signals();
        return 0;
    }

    int target_sig = SIGTERM;
    int pid_start_index = 1;

    // Parse signal flag options: '-s <sig>' or '-<sig>'
    if (argv[1][0] == '-') {
        if (strcmp(argv[1], "-s") == 0) {
            if (argc < 4) {
                print_usage();
                return 1;
            }
            target_sig = parse_signal(argv[2]);
            pid_start_index = 3;
        } else {
            target_sig = parse_signal(&argv[1][1]);
            pid_start_index = 2;
        }

        if (target_sig < 1 || target_sig >= NSIG) {
            printf("kill: unknown signal '%s'\n", argv[1]);
            return 1;
        }
    }

    if (pid_start_index >= argc) {
        printf("kill: no process specified\n");
        print_usage();
        return 1;
    }

    int errors = 0;
    for (int i = pid_start_index; i < argc; i++) {
        int pid = atoi(argv[i]);
        if (pid <= 0) {
            printf("kill: invalid pid '%s'\n", argv[i]);
            errors++;
            continue;
        }

        if (kill((uint32_t)pid, target_sig) < 0) {
            printf("kill: (%d) - No such process\n", pid);
            errors++;
        }
    }

    return (errors > 0) ? 1 : 0;
}