#include "../libs/meow_libc.h"

DESCRIPTION("reboot.elf: Reboot the system");

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("Rebooting the system...\n");
    reboot();

    printf("Reboot failed. Halting CPU.\n");
    while(1) {
        sys_yield();
    }

    return 1;
}