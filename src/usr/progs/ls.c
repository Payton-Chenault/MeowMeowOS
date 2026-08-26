#include "../libs/meow_libc.h"

#define MODULE "LS"

DESCRIPTION("ls.elf: List directory contents");

extern void log_trace(const char *module, const char *fmt, ...);

int main(int argc, char **argv) {
    const char *target_dir = (argc > 1) ? argv[1] : ".";
    log_trace(MODULE, "Listing directory: '%s'", target_dir);


    int ret = sys_list_dir(target_dir);
    if (ret != 0) {
        printf("ls: cannot access '%s': No such file or directory\n", target_dir);
        log_trace(MODULE, "Failed to list directory: %s", target_dir);
        return 1;
    }

    printf("--------------------------------------------------\n");
    return 0;
}