#include "../libs/meow_libc.h"

DESCRIPTION("ls.elf: List directory contents");

int main(int argc, char **argv) {
    const char *target_dir = (argc > 1) ? argv[1] : ".";
    sys_list_dir(target_dir);
    return 0;
}