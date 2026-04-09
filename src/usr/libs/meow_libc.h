#ifndef MEOW_LIBC_H
#define MEOW_LIBC_H

#include "../../kernel/syscall/syscall.h"

static inline void sys_print(const char* str) {
    __asm__ volatile (
        "int $0x80"
        : 
        : "a"(SYS_PRINT), "b"(str)
    );
}

// Wrapper for Syscall 2: Yield
static inline void sys_yield() {
    __asm__ volatile (
        "int $0x80"
        :
        : "a"(SYS_YIELD)
    );
}

static inline char sys_read_char() {
    char c;
    __asm__ volatile (
        "int $0x80"
        : "=a"(c)
        : "a"(SYS_READ_CHAR)
    );
    return c; 
}

static inline void sys_print_char(char c) {
    char str[2] = {c, '\0'};
    sys_print(str); 
}

static inline void sys_exit() {
    __asm__ volatile (
        "int $0x80"
        : 
        : "a"(SYS_RETURN)
    );
}

#endif