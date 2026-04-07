#include "kernel_services.h"
#include "../utils/console_print/kconsole.h"
#include "../arch/x86/pit/pit.h"
#include "../mem/heap/heap.h"
#include "../lib/integer_ascii_converters/itoa.h"

#define MODULE "KERNEL_SERVICES"

void kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char* p = fmt; *p != '\0'; p++) {
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

void kpanic(const char* msg) {
    log_error(MODULE, "Kernel Panic!\nThe following might aid in the reason of this panic: [%s]\nHALTING! Please Restart System To Reboot", msg);
    __asm__ volatile("cli");
    __asm__ volatile("hlt");
}

void ksleep(uint32_t ms) {
    volatile uint32_t system_ticks = get_ticks();
    uint32_t timer_frequency = get_system_freq();

    uint32_t wait_ticks = (ms * timer_frequency) / 1000;
    uint32_t target_ticks = system_ticks + wait_ticks;

    while (get_ticks() < target_ticks) {
        __asm__ volatile("hlt");
    }
}

void* kmem_alloc(size_t size) {
    return mem_alloc(size);
}

void kmem_free(void* ptr) {
    mem_free(ptr);
}

