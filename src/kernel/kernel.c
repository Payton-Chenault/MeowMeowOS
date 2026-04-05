#include "kernel.h"


void kernel_main() {
    const char* splash_screen = " _____                   _____                   _____ _____\n|     |___ ___ _ _ _ ___|     |___ ___ _ _ _ ___|     |   __|\n| | | | -_| . | | | |___| | | | -_| . | | | |___|  |  |__   |\n|_|_|_|___|___|_____|   |_|_|_|___|___|_____|   |_____|_____|\n";
    
    init_idt();
    logger_init(LOG_LEVEL_INFO);
    keyboard_initialize();
    __asm__ volatile("sti");


    kinit_screen();
    kprintln(splash_screen);
}