#include "../libs/meow_libc.h"

DESCRIPTION("free.elf: Display amount of free and used memory in the system");

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    sys_mem_info_t info;
    if (sys_get_mem_info(&info) < 0) {
        printf("free: failed to get memory info\n");
        return 1;
    }

    unsigned int total_kb = info.total_bytes / 1024;
    unsigned int used_kb  = info.used_bytes / 1024;
    unsigned int free_kb  = info.free_bytes / 1024;

    printf("total(KB)   used(KB)    free(KB)\n");
    printf("%d      %d       %d\n", total_kb, used_kb, free_kb);
    
    return 0;
}