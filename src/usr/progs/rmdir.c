#include "../libs/meow_libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: rmdir <dir>\n");
        return 1;
    }

    if (rmdir(argv[1]) != 0) {
        perror("rmdir");
        return 1;
    }

    return 0;
}