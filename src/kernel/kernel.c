#include "kernel.h"


void kernel_main() {
    const char* splash_screen = " _____                   _____                   _____ _____\n|     |___ ___ _ _ _ ___|     |___ ___ _ _ _ ___|     |   __|\n| | | | -_| . | | | |___| | | | -_| . | | | |___|  |  |__   |\n|_|_|_|___|___|_____|   |_|_|_|___|___|_____|   |_____|_____|\n";
    
    gdt_initialize();
    idt_initialize();
    kscreen_initialize();
    keyboard_initialize();





    enable_interrupts();


    kprintln(splash_screen);

    char line[64];
    while(1) {
        kprint("> ");
        keyboard_read_line(line, 64);
        kprint("\nYou typed: ");
        kprintln(line);
    }
}