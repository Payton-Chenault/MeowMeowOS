#include "meow_libc.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LIBC_MAX_FDS 32
#define STDIO_BUF_SIZE 4096

typedef struct {
    uint8_t buffer[STDIO_BUF_SIZE];
    size_t head;
    size_t tail;
    bool eof;
} file_buffer_t;

static file_buffer_t fd_buffers[LIBC_MAX_FDS];

static void flush_fd_buffer(int fd) {
    if (fd >= 0 && fd < LIBC_MAX_FDS) {
        fd_buffers[fd].head = 0;
        fd_buffers[fd].tail = 0;
        fd_buffers[fd].eof = false;
    }
}

/* File descriptor API with 4KB block-level read caching */

int open(const char *pathname) {
    int fd = sys_open(pathname);
    if (fd >= 0 && fd < LIBC_MAX_FDS) {
        flush_fd_buffer(fd);
    }
    return fd;
}

int close(int fd) {
    flush_fd_buffer(fd);
    return sys_close(fd);
}

int read(int fd, void *buf, size_t count) {
    if (buf == NULL || count == 0) return 0;
    if (fd < 0 || fd >= LIBC_MAX_FDS) {
        return sys_read(fd, buf, count);
    }

    file_buffer_t *fb = &fd_buffers[fd];
    size_t bytes_read = 0;
    uint8_t *dst = (uint8_t *)buf;

    // 1. Drain available buffered data first
    while (fb->head < fb->tail && bytes_read < count) {
        dst[bytes_read++] = fb->buffer[fb->head++];
    }

    if (bytes_read == count) {
        return (int)bytes_read;
    }

    // 2. Direct read if remaining request is large
    size_t remaining = count - bytes_read;
    if (remaining >= STDIO_BUF_SIZE) {
        int r = sys_read(fd, dst + bytes_read, remaining);
        if (r > 0) {
            bytes_read += r;
        } else if (bytes_read == 0) {
            return r;
        }
        return (int)bytes_read;
    }

    // 3. Refill the 4KB buffer from the kernel
    if (remaining > 0 && !fb->eof) {
        int r = sys_read(fd, fb->buffer, STDIO_BUF_SIZE);
        if (r <= 0) {
            fb->eof = true;
            return (bytes_read > 0) ? (int)bytes_read : r;
        }
        fb->head = 0;
        fb->tail = (size_t)r;

        while (fb->head < fb->tail && bytes_read < count) {
            dst[bytes_read++] = fb->buffer[fb->head++];
        }
    }

    return (int)bytes_read;
}

int write(int fd, const void *buf, size_t count) {
    return sys_write(fd, buf, count);
}

long lseek(int fd, long offset, int whence) {
    if (fd >= 0 && fd < LIBC_MAX_FDS) {
        if (whence == SEEK_CUR) {
            long unread = (long)(fd_buffers[fd].tail - fd_buffers[fd].head);
            offset -= unread;
        }
        flush_fd_buffer(fd);
    }
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(SYS_LSEEK), "b"(fd), "c"((int32_t)offset), "d"(whence)
                     : "memory");
    return ret;
}

int stat(const char *pathname, sys_stat_t *buf) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(SYS_STAT), "b"(pathname), "c"(buf)
                     : "memory");
    return ret;
}

int fstat(int fd, sys_stat_t *buf) {
    if (buf == NULL || fd < 0) return -1;
    memset(buf, 0, sizeof(sys_stat_t));
    long cur = lseek(fd, 0, SEEK_CUR);
    long end = lseek(fd, 0, SEEK_END);
    lseek(fd, cur, SEEK_SET);
    if (end < 0) return -1;
    buf->size = (uint32_t)end;
    buf->type = 1;
    return 0;
}

int dup(int oldfd) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_DUP), "b"(oldfd));
    return ret;
}

int dup2(int oldfd, int newfd) {
    flush_fd_buffer(newfd);
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(SYS_DUP2), "b"(oldfd), "c"(newfd)
                     : "memory");
    return ret;
}

int mkdir(const char *pathname) { return sys_mkdir(pathname); }
int rmdir(const char *pathname) { return sys_rmdir(pathname); }
int unlink(const char *pathname) { return sys_remove(pathname); }
int chdir(const char *path) { return sys_chdir(path); }

char *getcwd(char *buf, size_t size) {
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(SYS_GETCWD), "b"(buf), "c"(size)
                     : "memory");
    return (ret >= 0) ? buf : NULL;
}

/* Stdio I/O functions */

int fgetc(int fd) {
    if (fd < 0 || fd >= LIBC_MAX_FDS) {
        uint8_t c;
        int r = sys_read(fd, &c, 1);
        return (r > 0) ? (int)c : EOF;
    }

    file_buffer_t *fb = &fd_buffers[fd];

    if (fb->head >= fb->tail) {
        if (fb->eof) return EOF;

        int bytes = sys_read(fd, fb->buffer, STDIO_BUF_SIZE);
        if (bytes <= 0) {
            fb->eof = true;
            return EOF;
        }
        fb->head = 0;
        fb->tail = (size_t)bytes;
    }

    return (int)fb->buffer[fb->head++];
}

int getchar(void) {
    return fgetc(0);
}

char *fgets(char *s, int size, int fd) {
    if (s == NULL || size <= 0) return NULL;

    int i = 0;
    while (i < size - 1) {
        int ch = fgetc(fd);
        if (ch == EOF) {
            if (i == 0) return NULL;
            break;
        }
        s[i++] = (char)ch;
        if (ch == '\n') {
            break;
        }
    }
    s[i] = '\0';
    return s;
}

int fputs(const char *s, int fd) {
    if (!s) return EOF;
    size_t len = strlen(s);
    return write(fd, s, len);
}

int putchar(int c) {
    char ch = (char)c;
    return write(1, &ch, 1);
}

int puts(const char *str) {
    if (!str) return EOF;
    fputs(str, 1);
    putchar('\n');
    return 0;
}

/* Formatted print engine */

int vsnprintf(char *str, size_t size, const char *fmt, va_list args) {
    if (str == NULL || size == 0) return 0;
    size_t len = 0;

    for (const char *p = fmt; *p != '\0' && len < size - 1; p++) {
        if (*p != '%') {
            str[len++] = *p;
            continue;
        }

        p++;
        if (*p == '\0') break;

        switch (*p) {
        case 'c': {
            str[len++] = (char)va_arg(args, int);
            break;
        }
        case 's': {
            const char *s = va_arg(args, const char *);
            if (!s) s = "(null)";
            while (*s != '\0' && len < size - 1) {
                str[len++] = *s++;
            }
            break;
        }
        case 'd':
        case 'i': {
            char numbuf[32];
            itoa(va_arg(args, int), numbuf, 10);
            for (int i = 0; numbuf[i] != '\0' && len < size - 1; i++) {
                str[len++] = numbuf[i];
            }
            break;
        }
        case 'u': {
            char numbuf[32];
            itoa((int)va_arg(args, unsigned int), numbuf, 10);
            for (int i = 0; numbuf[i] != '\0' && len < size - 1; i++) {
                str[len++] = numbuf[i];
            }
            break;
        }
        case 'x':
        case 'p': {
            char numbuf[32];
            itoa(va_arg(args, uint32_t), numbuf, 16);
            if (*p == 'p' && len + 2 < size - 1) {
                str[len++] = '0';
                str[len++] = 'x';
            }
            for (int i = 0; numbuf[i] != '\0' && len < size - 1; i++) {
                str[len++] = numbuf[i];
            }
            break;
        }
        case '%': {
            str[len++] = '%';
            break;
        }
        default: {
            str[len++] = '%';
            if (len < size - 1) {
                str[len++] = *p;
            }
            break;
        }
        }
    }

    str[len] = '\0';
    return (int)len;
}

int snprintf(char *str, size_t size, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(str, size, fmt, args);
    va_end(args);
    return ret;
}

int sprintf(char *str, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(str, 0x7FFFFFFF, fmt, args);
    va_end(args);
    return ret;
}

int vprintf(const char *fmt, va_list ap) {
    char buf[1024];
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (len > 0) {
        write(1, buf, len);
    }
    return len;
}

int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vprintf(fmt, ap);
    va_end(ap);
    return ret;
}

/* TTY termios API */

int tcgetattr(int fd, struct termios *termios_p) {
    (void)fd;
    if (!termios_p) return -1;
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(20), "b"(termios_p)
                     : "memory");
    return ret;
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
    (void)fd;
    (void)optional_actions;
    if (!termios_p) return -1;
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(21), "b"(termios_p)
                     : "memory");
    return ret;
}

/* User-space logging API */

static void user_log_v(int level, const char *module, const char *fmt, va_list ap) {
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    sys_log(level, module ? module : "USER", buf);
}

void log_trace(const char *module, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    user_log_v(LOG_LEVEL_TRACE, module, fmt, ap);
    va_end(ap);
}

void log_debug(const char *module, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    user_log_v(LOG_LEVEL_DEBUG, module, fmt, ap);
    va_end(ap);
}

void log_info(const char *module, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    user_log_v(LOG_LEVEL_INFO, module, fmt, ap);
    va_end(ap);
}

void log_warn(const char *module, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    user_log_v(LOG_LEVEL_WARNING, module, fmt, ap);
    va_end(ap);
}

void log_error(const char *module, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    user_log_v(LOG_LEVEL_ERROR, module, fmt, ap);
    va_end(ap);
}

int pipe(int pipefd[2]) {
    if (pipefd == NULL) return -1;
    return sys_pipe(pipefd);
}