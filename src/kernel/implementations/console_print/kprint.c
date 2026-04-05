#include "kprint.h"

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

void kinit_screen() {
    terminal_initialize();
}