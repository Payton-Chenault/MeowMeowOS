#include "meow_libc.h"

static int vsnprintf(char *str, size_t size, const char *fmt, va_list args) {
    if (size == 0) {
        return 0;
    }

    size_t written = 0;
    const char *p = fmt;

    while (*p && written < size - 1) {
        if (*p != '%') {
            str[written++] = *p++;
            continue;
        }

        p++;
        if (*p == '\0') {
            break;
        }

        switch (*p) {
        case 'c': {
            char c = (char)va_arg(args, int);
            str[written++] = c;
            break;
        }
        case 's': {
            const char *s = va_arg(args, const char *);
            if (!s) {
                s = "(null)";
            }
            while (*s && written < size - 1) {
                str[written++] = *s++;
            }
            break;
        }
        case 'd':
        case 'i': {
            int num = va_arg(args, int);
            char numbuf[32];
            itoa(num, numbuf, 10);
            for (int i = 0; numbuf[i] && written < size - 1; i++) {
                str[written++] = numbuf[i];
            }
            break;
        }
        case 'u': {
            unsigned int num = va_arg(args, unsigned int);
            char numbuf[32];
            itoa((int)num, numbuf, 10);
            for (int i = 0; numbuf[i] && written < size - 1; i++) {
                str[written++] = numbuf[i];
            }
            break;
        }
        case 'x':
        case 'X': {
            unsigned int num = va_arg(args, unsigned int);
            char numbuf[32];
            itoa((int)num, numbuf, 16);
            for (int i = 0; numbuf[i] && written < size - 1; i++) {
                str[written++] = numbuf[i];
            }
            break;
        }
        case '%': {
            str[written++] = '%';
            break;
        }
        default: {
            str[written++] = '%';
            if (written < size - 1) {
                str[written++] = *p;
            }
            break;
        }
        }

        p++;
    }

    str[written] = '\0';
    return (int)written;
}

int putchar(int c) {
    char ch = (char)c;
    write(1, &ch, 1);
    return c;
}

int puts(const char *str) {
    int n = write(1, str, strlen(str));
    if (n < 0) {
        return n;
    }

    if (write(1, "\n", 1) < 0) {
        return -1;
    }

    return n + 1;
}

int printf(const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        write(1, buf, (size_t)len);
    }

    return len;
}

int sprintf(char *str, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(str, (size_t)-1, fmt, args);
    va_end(args);
    return len;
}

int snprintf(char *str, size_t size, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(str, size, fmt, args);
    va_end(args);
    return len;
}

int open(const char *pathname) { return sys_open(pathname); }
int close(int fd) { return sys_close(fd); }
int read(int fd, void *buf, size_t count) { return sys_read(fd, buf, (unsigned int)count); }
int write(int fd, const void *buf, size_t count) { return sys_write(fd, buf, (unsigned int)count); }
int mkdir(const char *pathname) { return sys_mkdir(pathname); }
int rmdir(const char *pathname) { return sys_rmdir(pathname); }
int unlink(const char *pathname) { return sys_remove(pathname); }
int chdir(const char *path) { return sys_chdir(path); }