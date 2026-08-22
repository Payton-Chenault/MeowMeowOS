#include "../libs/meow_libc.h"

DESCRIPTION("mkdir.elf: Create directory");

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: mkdir <dir>\n");
        return 1;
    }

    if (mkdir(argv[1]) != 0) {
        perror("mkdir");
        return 1;
    }

    return 0;
}