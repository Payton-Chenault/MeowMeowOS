#ifndef MEOW_LIBC_H
#define MEOW_LIBC_H

#include "../../kernel/syscall/syscall.h"

static inline unsigned int strlen(const char* str) {
    unsigned int len = 0;
    while (str[len]) len++;
    return len;
}

static inline void sys_yield() {
    __asm__ volatile (
        "int $0x80"
        :
        : "a"(SYS_YIELD)
    );
}

static inline void sys_exit() {
    __asm__ volatile (
        "int $0x80"
        : 
        : "a"(SYS_RETURN)
    );
}

static inline int sys_open(const char* filename) {
    int fd = -1;
    __asm__ volatile (
        "pushl %%ebx\n"
        "movl %2, %%ebx\n"
        "int $0x80\n"
        "popl %%ebx\n"
        : "=a"(fd)
        : "a"(SYS_OPEN), "c"(filename) 
    );
    return fd;  
}

static inline int sys_read(int fd, void* buffer, unsigned int size) {
    int bytes_read;
    __asm__ volatile (
        "int $0x80"
        : "=a"(bytes_read)
        : "a"(SYS_READ), "c"(fd), "d"(buffer), "S"(size)
    );
    return bytes_read;
}

static inline int sys_write(int fd, const void* buffer, unsigned int size) {
    int bytes_written;
    __asm__ volatile (
        "int $0x80"
        : "=a"(bytes_written)
        : "a"(7), "c"(fd), "d"(buffer), "S"(size)
    );
    return bytes_written;
}

static inline void sys_print(const char* str) {
    sys_write(1, str, strlen(str));
}

static inline char sys_read_char() {
    char c;
    sys_read(0, &c, 1);
    return c;
}

#endif