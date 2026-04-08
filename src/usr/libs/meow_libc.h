#ifndef MEOW_LIBC_H
#define MEOW_LIBC_H

static inline void sys_print(const char* str) {
    __asm__ volatile (
        "int $0x80"
        : 
        : "a"(1), "b"(str)
    );
}

// Wrapper for Syscall 2: Yield
static inline void sys_yield() {
    __asm__ volatile (
        "int $0x80"
        :
        : "a"(2)
    );
}

#endif