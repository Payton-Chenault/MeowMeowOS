#ifndef SYS_CALL_H
#define SYS_CALL_H

#include <stdint.h>

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

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct {
    uint32_t size;
    uint32_t type;
    uint32_t uid;
    uint32_t gid;
    uint16_t mode;
} sys_stat_t;

// --- Payload Structs ---
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
    char name[32];
} sys_process_info_t;

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

#endif