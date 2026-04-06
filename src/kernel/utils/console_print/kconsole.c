#include "kconsole.h"

#define MODULE "KERNEL_CONSOLE"
void kprint(const char *c) {
    terminal_print(c);
}

void kput_char(char c) {
    terminal_putchar(c);
}

void kclear_screen(void) {
    terminal_clear();
}

void kscreen_initialize() {
    terminal_initialize();
    log_debug(MODULE, "Kernel Console Initialized");
}

void kbackspace() {
    terminal_backspace();
}

void kprintln(const char *c) {
    kprint(c);
    kput_char('\n');
}

void kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    for (const char* p = format; *p != '\0'; p++) {
        if (*p != '%') {
            kput_char(*p);
            continue;
        }

        p++;
        switch (*p) {
            case 'c': {
                char c = (char)va_arg(args, int);
                kput_char(c);
                break;
            }
            case 's': {
                char* s = va_arg(args, char*);
                while (*s) kput_char(*s++);
                break;
            }
            case 'd': {
                int i = va_arg(args, int);
                char buffer[32];
                itoa(i, buffer, 10);
                for (int j = 0; buffer[j]; j++) kput_char(buffer[j]);
                break;
            }
            case 'x': {
                int x = va_arg(args, int);
                char buffer[32];
                itoa(x, buffer, 16);
                for (int j = 0; buffer[j]; j++) kput_char(buffer[j]);
                break;
            }
            case '%': {
                kput_char('%');
                break;
            }
            default:
                kput_char(*p);
                break;
        }
    }

    va_end(args);
}