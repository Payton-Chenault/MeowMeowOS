#include "kernel.h"

#define MODULE "KERNEL"

const char* splash_screen = " _____                   _____                   _____ _____\n|     |___ ___ _ _ _ ___|     |___ ___ _ _ _ ___|     |   __|\n| | | | -_| . | | | |___| | | | -_| . | | | |___|  |  |__   |\n|_|_|_|___|___|_____|   |_|_|_|___|___|_____|   |_____|_____|\n";

void kernel_main() {
    serial_logging_initialize(LOG_LEVEL_DEBUG);
    gdt_initialize();
    idt_initialize();   
    pmm_initialize_from_map(); 
    kscreen_initialize();
    keyboard_initialize();

    enable_interrupts();


    kprintln(splash_screen);

    void* buffer[40000];
    for (int i = 0; i < 40000; i++) {
        buffer[i] = pmm_alloc_block();
    }

    for (int i = 32000; i > 0; i--) {
        pmm_free_block(buffer[i]);
    }

    char line[64];
    while(1) {
        kprint("> ");
        keyboard_read_line(line, 64);
        kprint("\nYou typed: ");
        kprintln(line);
    }
}