#include "../libs/meow_libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: mkdir <dir>\n");
        return 1;
    }

    if (argv[1] == NULL) {
        printf("mkdir: argv[1] is NULL\n");
        return 1;
    }

    int rc = sys_mkdir(argv[1]);
    if (rc != 0) {
        printf("mkdir failed: %s\n", argv[1]);
        return 1;
    }

    return 0;
}