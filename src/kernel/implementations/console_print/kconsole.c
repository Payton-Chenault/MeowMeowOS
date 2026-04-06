#include "kconsole.h"

#define MODULE "KERNEL_CONSOLE"
void kprint(const char *c) {
    terminal_print(c);
}

void kprintln(const char *c) {
    terminal_print(c);
    terminal_putchar('\n');
}

void kput_char(char c) {
    terminal_putchar(c);
}

void kclear(void) {
    terminal_clear();
}

void kscreen_initialize() {
    terminal_initialize();
    log_debug(MODULE, "Kernel Console Initialized");
}

void kbackspace() {
    terminal_backspace();
}