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
    int fd;
    __asm__ volatile (
        "int $0x80"
        : "=a"(fd)
        : "a"(SYS_OPEN), "b"(filename)
        : "memory"
    );
    return fd;  
}

static inline int sys_read(int fd, void* buffer, unsigned int size) {
    int bytes_read;
    __asm__ volatile (
        "int $0x80"
        : "=a"(bytes_read)
        : "a"(SYS_READ), "b"(fd), "c"(buffer), "d"(size)
        : "memory"
    );
    return bytes_read;
}

static inline int sys_write(int fd, const void* buffer, unsigned int size) {
    int bytes_written;
    __asm__ volatile (
        "int $0x80"
        : "=a"(bytes_written)
        : "a"(SYS_WRITE), "b"(fd), "c"(buffer), "d"(size)
        : "memory"
    );
    return bytes_written;
}

static inline int sys_close(int fd) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_CLOSE), "b"(fd)
    );
    return ret;
}

static inline void sys_print(const char* str) {
    sys_write(1, str, strlen(str));
}

static inline char sys_read_char() {
    char c;
    sys_read(0, &c, 1);
    return c;
}

static inline int sys_format(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_FORMAT)
    );
    return ret;
}

#endif