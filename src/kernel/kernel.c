#include "kernel.h"

#include "kernel_services/kernel_services.h"

#define MODULE "KERNEL"

const char* splash_screen = " _____                   _____                   _____ _____\n|     |___ ___ _ _ _ ___|     |___ ___ _ _ _ ___|     |   __|\n| | | | -_| . | | | |___| | | | -_| . | | | |___|  |  |__   |\n|_|_|_|___|___|_____|   |_|_|_|___|___|_____|   |_____|_____|\n";

void kernel_bootstrap() {

    serial_logging_initialize(LOG_LEVEL_DEBUG);
    gdt_initialize();
    idt_initialize();   
    pit_initialize(1000);

    pmm_initialize_from_map(); 
    vmm_initialize();
    heap_initialize(0x600000, 0x100000);

    kscreen_initialize();
    keyboard_initialize();

    enable_interrupts();
}


void kernel_main() {
    kernel_bootstrap();

    kprintf(splash_screen);
    kprintf("MeowMeowOS is ready. Type 'help' for commands.\n");

    kshell_main();
}