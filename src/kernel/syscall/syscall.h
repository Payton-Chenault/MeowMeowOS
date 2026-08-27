#ifndef SYS_CALL_H
#define SYS_CALL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* ==========================================================================
 * Logging Level Constants
 * ========================================================================== */

#define SYSLOG_LEVEL_NONE    0
#define SYSLOG_LEVEL_ERROR   1
#define SYSLOG_LEVEL_WARNING 2
#define SYSLOG_LEVEL_INFO    3
#define SYSLOG_LEVEL_DEBUG   4
#define SYSLOG_LEVEL_TRACE   5

/* ==========================================================================
 * System Call Vector Numbers
 * ========================================================================== */

#define SYS_YIELD 1
#define SYS_RETURN 2
#define SYS_OPEN 3
#define SYS_CLOSE 4
#define SYS_READ 5
#define SYS_WRITE 6
#define SYS_FORMAT 7
#define SYS_LIST_DIR 8
#define SYS_MKDIR 9
#define SYS_RMDIR 10
#define SYS_REMOVE 11
#define SYS_CREATE 12
#define SYS_UPTIME 13
#define SYS_ALLOC_PAGE 14
#define SYS_FREE_PAGE 15
#define SYS_CHDIR  16
#define SYS_COPY_FILE  17
#define SYS_LSEEK 18
#define SYS_STAT 19
#define SYS_GETCWD 20
#define SYS_DUP 21
#define SYS_DUP2 22
#define SYS_GET_DESCRIPTION 23
#define SYS_SYSLOG 24
#define SYS_GET_MEM_INFO 25
#define SYS_GET_PROCESS_INFO 26
#define SYS_SET_PRIORITY 27
#define SYS_GET_PRIORITY 28
#define SYS_GET_TIME     29
#define SYS_SBRK         30
#define SYS_LOG          31
#define SYS_PIPE         32
#define SYS_KILL         33
#define SYS_SIGNAL       34
#define SYS_SET_BUSY     35
#define SYS_GET_PCI_DEVICES 36
#define SYS_POWEROFF     37
#define SYS_REBOOT       38

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* ==========================================================================
 * System Data Structures
 * ========================================================================== */

typedef struct {
    uint32_t size;
    uint32_t type;
    uint32_t uid;
    uint32_t gid;
    uint16_t mode;
} sys_stat_t;

typedef struct {
    uint32_t total_bytes;
    uint32_t used_bytes;
    uint32_t free_bytes;
} sys_mem_info_t;

typedef struct {
    uint32_t pid;
    uint32_t parent_pid;
    uint32_t state;
    uint32_t cpu_ticks;
    uint8_t base_priority;
    uint8_t dynamic_priority;
    char name[32];
} sys_process_info_t;

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} sys_time_t;

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision_id;
    uint8_t header_type;
    uint8_t irq_line;
    uint8_t irq_pin;
    uint32_t bar[6];
} sys_pci_device_t;

/* ==========================================================================
 * Unified Low-Level System Call Wrappers
 * ========================================================================== */

static inline void sys_yield(void) {
    __asm__ volatile("int $0x80" : : "a"(SYS_YIELD));
}

static inline void sys_exit(int status) {
    __asm__ volatile("int $0x80" : : "a"(SYS_RETURN), "b"(status));
}

static inline int sys_open(const char *filename) {
    int fd;
    __asm__ volatile("int $0x80" : "=a"(fd) : "a"(SYS_OPEN), "b"(filename) : "memory");
    return fd;
}

static inline int sys_read(int fd, void *buffer, unsigned int size) {
    int bytes_read;
    __asm__ volatile("int $0x80" : "=a"(bytes_read) : "a"(SYS_READ), "b"(fd), "c"(buffer), "d"(size) : "memory");
    return bytes_read;
}

static inline int sys_write(int fd, const void *buffer, unsigned int size) {
    int bytes_written;
    __asm__ volatile("int $0x80" : "=a"(bytes_written) : "a"(SYS_WRITE), "b"(fd), "c"(buffer), "d"(size) : "memory");
    return bytes_written;
}

static inline int sys_close(int fd) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_CLOSE), "b"(fd));
    return ret;
}

static inline int sys_format(void) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_FORMAT));
    return ret;
}

static inline int sys_list_dir(const char *path) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_LIST_DIR), "b"(path) : "memory");
    return ret;
}

static inline int sys_mkdir(const char *path) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_MKDIR), "b"(path) : "memory");
    return ret;
}

static inline int sys_rmdir(const char *path) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_RMDIR), "b"(path) : "memory");
    return ret;
}

static inline int sys_remove(const char *path) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_REMOVE), "b"(path) : "memory");
    return ret;
}

static inline int sys_create(const char *path) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_CREATE), "b"(path) : "memory");
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
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_CHDIR), "b"(path) : "memory");
    return ret;
}

static inline int sys_copy_file(const char *src, const char *dst) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_COPY_FILE), "b"(src), "c"(dst) : "memory");
    return ret;
}

static inline long sys_lseek(int fd, long offset, int whence) {
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_LSEEK), "b"(fd), "c"(offset), "d"(whence) : "memory");
    return ret;
}

static inline int sys_stat(const char *pathname, sys_stat_t *buf) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_STAT), "b"(pathname), "c"(buf) : "memory");
    return ret;
}

static inline char *sys_getcwd(char *buf, unsigned int size) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_GETCWD), "b"(buf), "c"(size) : "memory");
    return (ret >= 0) ? buf : NULL;
}

static inline int sys_dup(int oldfd) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_DUP), "b"(oldfd));
    return ret;
}

static inline int sys_dup2(int oldfd, int newfd) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_DUP2), "b"(oldfd), "c"(newfd));
    return ret;
}

static inline void sys_print(const char *str) {
    unsigned int len = 0;
    while (str && str[len] != '\0') len++;
    sys_write(1, str, len);
}

static inline char sys_read_char(void) {
    char c;
    sys_read(0, &c, 1);
    return c;
}

static inline int sys_get_description(const char *path, char *buffer, unsigned int size) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_GET_DESCRIPTION), "b"(path), "c"(buffer), "d"(size) : "memory");
    return ret;
}

static inline int sys_syslog(char *buffer, unsigned int size) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_SYSLOG), "b"(buffer), "c"(size) : "memory");
    return ret;
}

static inline int sys_pipe(int pipefd[2]) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_PIPE), "b"(pipefd) : "memory");
    return ret;
}

static inline int sys_get_mem_info(sys_mem_info_t *info) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_GET_MEM_INFO), "b"(info) : "memory");
    return ret;
}

static inline int sys_get_process_info(sys_process_info_t *buffer, unsigned int max_entries) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_GET_PROCESS_INFO), "b"(buffer), "c"(max_entries) : "memory");
    return ret;
}

static inline int sys_set_priority(uint32_t pid, uint8_t priority) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_SET_PRIORITY), "b"(pid), "c"(priority) : "memory");
    return ret;
}

static inline int sys_get_priority(uint32_t pid) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_GET_PRIORITY), "b"(pid) : "memory");
    return ret;
}

static inline int sys_get_time(sys_time_t *time) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_GET_TIME), "b"(time) : "memory");
    return ret;
}

static inline void *sys_sbrk(int32_t increment) {
    int32_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_SBRK), "b"(increment) : "memory");
    return (void *)ret;
}

static inline int sys_log(int level, const char *module, const char *msg) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_LOG), "b"(level), "c"(module), "d"(msg) : "memory");
    return ret;
}

static inline int sys_kill(uint32_t pid, int sig) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_KILL), "b"(pid), "c"(sig) : "memory");
    return ret;
}

static inline void *sys_signal(int sig, void *handler) {
    void *ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_SIGNAL), "b"(sig), "c"(handler) : "memory");
    return ret;
}

static inline int sys_set_busy(bool is_busy) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_SET_BUSY), "b"(is_busy) : "memory");
    return ret;
}

static inline int sys_get_pci_devices(sys_pci_device_t *buffer, unsigned int max_entries) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_GET_PCI_DEVICES), "b"(buffer), "c"(max_entries) : "memory");
    return ret;
}

static inline int sys_poweroff(void) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_POWEROFF));
    return ret;
}

static inline int sys_reboot(void) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYS_REBOOT));
    return ret;
}

#endif