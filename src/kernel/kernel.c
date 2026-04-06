#include "kernel.h"

#define MODULE "KERNEL"

const char* splash_screen = " _____                   _____                   _____ _____\n|     |___ ___ _ _ _ ___|     |___ ___ _ _ _ ___|     |   __|\n| | | | -_| . | | | |___| | | | -_| . | | | |___|  |  |__   |\n|_|_|_|___|___|_____|   |_|_|_|___|___|_____|   |_____|_____|\n";

void kernel_main() {
    serial_logging_initialize(LOG_LEVEL_INFO);
    gdt_initialize();
    idt_initialize();   
    pmm_initialize_from_map(); 
    vmm_initialize();
    heap_initialize(0x600000, 0x100000);
    kscreen_initialize();
    keyboard_initialize();

    enable_interrupts();


    kprintln(splash_screen);

       log_info("TEST", "Starting Heap Stress Test...");

    void* a = kmalloc(128);
    void* b = kmalloc(256);
    void* c = kmalloc(128);

    log_debug("TEST", "Allocated A(128), B(256), C(128)");

    // Free the middle one and the last one
    kfree(b);
    kfree(c); 
    // At this point, B and C should have merged into one ~384+ byte block

    // Try to allocate something that fits in the combined B+C gap
    void* d = kmalloc(300);
    
    if (d) {
        log_info("TEST", "Heap Coalescing Test PASSED! Found space at 0x%x", d);
        kfree(a);
        kfree(d);
    } else {
        log_error("TEST", "Heap Coalescing Test FAILED: Could not find space.");
    }

    char line[64];
    while(1) {
        kprint("> ");
        keyboard_read_line(line, 64);
        kprint("\nYou typed: ");
        kprintln(line);
    }
}