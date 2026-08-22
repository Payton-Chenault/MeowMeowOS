#include "../libs/meow_libc.h"

DESCRIPTION("uptime.elf: Show system uptime");

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    unsigned int ticks = sys_uptime();
    printf("%u\n", ticks);

    return 0;
}