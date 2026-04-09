#ifndef MEOW_LIBC_H
#define MEOW_LIBC_H

#include "../../kernel/syscall/syscall.h"



// Wrapper for Syscall 2: Yield
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

#endif