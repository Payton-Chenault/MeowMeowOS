#include "../libs/meow_libc.h"

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("Are you sure you want to format the drive? This will erase all data. <y/N>: ");

    int c = getchar();
    putchar('\n');

    if (c == 'y' || c == 'Y') {
        sys_format();
    }

    return 0;
}