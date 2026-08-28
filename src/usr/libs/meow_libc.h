#ifndef MEOW_LIBC_H
#define MEOW_LIBC_H

#include "../../kernel/syscall/syscall.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Error codes */
#define EPERM           1
#define ENOENT          2
#define EIO             5
#define EBADF           9
#define ENOMEM          12
#define EACCES          13
#define EFAULT          14
#define EINVAL          22

#define EOF (-1)

#define TLS_ERRNO_ADDR  ((volatile int *)0xBFFFE000)
#define errno (*TLS_ERRNO_ADDR)

#define DESCRIPTION(text) \
    static const char meow_description[] __attribute__((used)) = "@DESC:" text; \
    static void __attribute__((constructor)) __force_desc(void) { \
        volatile const char *p = meow_description; \
        (void)p; \
    }

/* POSIX Signal Definitions */
#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGFPE    8
#define SIGKILL   9
#define SIGSEGV   11
#define SIGALRM   14
#define SIGTERM   15
#define NSIG      32

typedef void (*sighandler_t)(int);

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

/* Terminal structures */
#define ICANON  0x0002
#define ECHO    0x0008
#define ISIG    0x0001

struct termios {
    unsigned int c_iflag;
    unsigned int c_oflag;
    unsigned int c_cflag;
    unsigned int c_lflag;
    unsigned char c_cc[32];
};

typedef sys_pci_device_t pci_device_t;
typedef sys_mouse_state_t mouse_state_t;

/* getopt */
extern int getopt(int argc, char * const argv[], const char *opts);
extern int opterr, optind, optopt, optreset;
extern char *optarg;

/* String functions */
size_t strlen(const char *str);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);
char *strdup(const char *s);
char *strndup(const char *s, size_t n);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
char *strpbrk(const char *s, const char *accept);
char *strtok(char *str, const char *delim);
char *strtok_r(char *str, const char *delim, char **saveptr);

/* Memory functions */
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *ptr, int value, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memchr(const void *s, int c, size_t n);

/* stdlib functions */
void exit(int status);
int atoi(const char *str);
long strtol(const char *str, char **endptr, int base);
char *itoa(int value, char *str, int base);
const char *strerror(int errnum);
void perror(const char *s);
void *sbrk(intptr_t increment);
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t count, size_t size);
void *realloc(void *ptr, size_t new_size);
void *reallocarray(void *ptr, size_t nmemb, size_t size);
size_t malloc_usable_size(void *ptr);

/* Signal API */
static inline int kill(uint32_t pid, int sig) {
    return sys_kill(pid, sig);
}

static inline sighandler_t signal(int signum, sighandler_t handler) {
    return (sighandler_t)sys_signal(signum, (void *)handler);
}

static inline int raise(int sig) {
    sys_process_info_t info;
    sys_get_process_info(&info, 1);
    return sys_kill(info.pid, sig);
}

/* PCI & System Power APIs */
static inline int get_pci_devices(pci_device_t *buffer, unsigned int max_entries) {
    return sys_get_pci_devices(buffer, max_entries);
}

static inline int poweroff(void) {
    return sys_poweroff();
}

static inline int reboot(void) {
    return sys_reboot();
}

/* stdio functions */
int putchar(int c);
int puts(const char *str);
int printf(const char *fmt, ...);
int sprintf(char *str, const char *fmt, ...);
int snprintf(char *str, size_t size, const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int vsnprintf(char *str, size_t size, const char *fmt, va_list args);
int getchar(void);
int fgetc(int fd);
char *fgets(char *s, int size, int fd);
int fputs(const char *s, int fd);
int fflush(int fd);

/* File descriptor API */
int open(const char *pathname);
int close(int fd);
int read(int fd, void *buf, size_t count);
int write(int fd, const void *buf, size_t count);
int mkdir(const char *pathname);
int pipe(int pipefd[2]);
int rmdir(const char *pathname);
int unlink(const char *pathname);
int chdir(const char *path);
long lseek(int fd, long offset, int whence);
int stat(const char *pathname, sys_stat_t *buf);
int fstat(int fd, sys_stat_t *buf);
int dup(int oldfd);
int dup2(int oldfd, int newfd);
char *getcwd(char *buf, size_t size);

/* TTY Control functions */
int tcgetattr(int fd, struct termios *termios_p);
int tcsetattr(int fd, int optional_actions, const struct termios *termios_p);

/* Logging functions */
void log_trace(const char *module, const char *fmt, ...);
void log_debug(const char *module, const char *fmt, ...);
void log_info(const char *module, const char *fmt, ...);
void log_warning(const char *module, const char *fmt, ...);
void log_error(const char *module, const char *fmt, ...);

/* Networking functions */
uint32_t inet_addr(const char *ip_str);
static inline int sys_ping(uint32_t target_ip, uint32_t *latency_out) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(39), "b"(target_ip), "c"(latency_out) : "memory");
    return ret;
}

#endif