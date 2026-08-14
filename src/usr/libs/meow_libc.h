#ifndef MEOW_LIBC_H
#define MEOW_LIBC_H

#include "../../kernel/syscall/syscall.h"
#include <stdarg.h>
#include <stddef.h>

static inline unsigned int strlen(const char *str) {
  unsigned int len = 0;
  while (str[len])
    len++;
  return len;
}

// Utility function to support extensive logging
static inline void itoa(int n, char *buffer, int base) {
  int i = 0;
  int is_negative = 0;
  if (n == 0) {
    buffer[i++] = '0';
    buffer[i] = '\0';
    return;
  }
  if (n < 0 && base == 10) {
    is_negative = 1;
    n = -n;
  }
  while (n != 0) {
    int rem = n % base;
    buffer[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
    n = n / base;
  }
  if (is_negative)
    buffer[i++] = '-';
  buffer[i] = '\0';
  int start = 0, end = i - 1;
  while (start < end) {
    char temp = buffer[start];
    buffer[start] = buffer[end];
    buffer[end] = temp;
    start++;
    end--;
  }
}

static inline void sys_yield() {
  __asm__ volatile("int $0x80" : : "a"(SYS_YIELD));
}

static inline void sys_exit() {
  __asm__ volatile("int $0x80" : : "a"(SYS_RETURN));
}

static inline int sys_open(const char *filename) {
  int fd;
  __asm__ volatile("int $0x80"
                   : "=a"(fd)
                   : "a"(SYS_OPEN), "b"(filename)
                   : "memory");
  return fd;
}

static inline int sys_read(int fd, void *buffer, unsigned int size) {
  int bytes_read;
  __asm__ volatile("int $0x80"
                   : "=a"(bytes_read)
                   : "a"(SYS_READ), "b"(fd), "c"(buffer), "d"(size)
                   : "memory");
  return bytes_read;
}

static inline int sys_write(int fd, const void *buffer, unsigned int size) {
  int bytes_written;
  __asm__ volatile("int $0x80"
                   : "=a"(bytes_written)
                   : "a"(SYS_WRITE), "b"(fd), "c"(buffer), "d"(size)
                   : "memory");
  return bytes_written;
}

static inline int sys_close(int fd) {
  int ret;
  __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_CLOSE), "b"(fd));
  return ret;
}

static inline void sys_print(const char *str) {
  sys_write(1, str, strlen(str));
}

static inline char sys_read_char() {
  char c;
  sys_read(0, &c, 1);
  return c;
}

static inline int sys_format(void) {
  int ret;
  __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_FORMAT));
  return ret;
}

static inline int sys_list_dir(const char *path) {
  int ret;
  __asm__ volatile("int $0x80"
                   : "=a"(ret)
                   : "a"(SYS_LIST_DIR), "b"(path)
                   : "memory");
  return ret;
}

static inline int sys_mkdir(const char *path) {
  int ret;
  __asm__ volatile("int $0x80"
                   : "=a"(ret)
                   : "a"(SYS_MKDIR), "b"(path)
                   : "memory");
  return ret;
}

static inline int sys_rmdir(const char *path) {
  int ret;
  __asm__ volatile("int $0x80"
                   : "=a"(ret)
                   : "a"(SYS_RMDIR), "b"(path)
                   : "memory");
  return ret;
}

static inline int sys_remove(const char *path) {
  int ret;
  __asm__ volatile("int $0x80"
                   : "=a"(ret)
                   : "a"(SYS_REMOVE), "b"(path)
                   : "memory");
  return ret;
}

static inline int sys_create(const char *path) {
  int ret;
  __asm__ volatile("int $0x80"
                   : "=a"(ret)
                   : "a"(SYS_CREATE), "b"(path)
                   : "memory");
  return ret;
}

static inline unsigned int sys_uptime(void) {
  unsigned int ticks;
  __asm__ volatile("int $0x80" : "=a"(ticks) : "a"(SYS_UPTIME));
  return ticks;
}

static inline void *sys_alloc_page(void) {
  void *ptr;
  __asm__ volatile("int $0x80" : "=a"(ptr) : "a"(SYS_ALLOC_PAGE));
  return ptr;
}

static inline int sys_free_page(void *ptr) {
  int ret;
  __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_FREE_PAGE), "b"(ptr));
  return ret;
}

static inline int sys_chdir(const char *path) {
  int ret;
  __asm__ volatile("int $0x80"
                   : "=a"(ret)
                   : "a"(SYS_CHDIR), "b"(path)
                   : "memory");
  return ret;
}

static inline int sys_copy_file(const char *src, const char *dst) {
  int ret;
  __asm__ volatile("int $0x80"
                   : "=a"(ret)
                   : "a"(SYS_COPY_FILE), "b"(src), "c"(dst)
                   : "memory");
  return ret;
}

static inline int snprintf(char* str, size_t size, const char* format, ...) {
    if (size == 0) return 0;
    size_t written = 0;
    va_list args;
    va_start(args, format);

    while (*format && written < size - 1) {
        if (*format == '%') {
            format++;
            if (*format == 's') {
                const char* s = va_arg(args, const char*);
                if (!s) s = "(null)";
                while (*s && written < size - 1) {
                    str[written++] = *s++;
                }
            } else if (*format == 'd' || *format == 'i') {
                int num = va_arg(args, int);
                char numbuf[32];
                itoa(num, numbuf, 10);
                for (int i = 0; numbuf[i] && written < size - 1; i++) {
                    str[written++] = numbuf[i];
                }
            } else if (*format == '%') {
                if (written < size - 1) str[written++] = '%';
            }
            format++;
        } else {
            if (written < size - 1) str[written++] = *format++;
        }
    }

    str[written] = '\0';
    va_end(args);
    return written;
}

#endif