#include "../libs/meow_libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: rm <file>\n");
        return 1;
    }

    if (unlink(argv[1]) != 0) {
        perror("rm");
        return 1;
    }

    return 0;
}