#include "../libs/meow_libc.h"

DESCRIPTION("touch.elf: Create empty file");

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: touch <file>\n");
        return 1;
    }

    int fd = sys_create(argv[1]);
    if (fd < 0) {
        perror("touch");
        return 1;
    }

    close(fd);
    return 0;
}