#include "../libs/meow_libc.h"

DESCRIPTION("poweroff.elf: Halt the system and power off");

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("Powering off the system...\n");
    poweroff();

    printf("ACPI poweroff failed or unsupported on this hardware. Halting CPU.\n");
    while(1) {
        sys_yield();
    }

    return 1;
}