#include "../libs/meow_libc.h"

#define MODULE "FREE"

DESCRIPTION("free.elf: Display amount of free and used memory in the system");

extern void log_trace(const char *module, const char *fmt, ...);
extern void log_debug(const char *module, const char *fmt, ...);
extern void log_info(const char *module, const char *fmt, ...);

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    log_info(MODULE, "Starting free memory check...");
    
    log_debug(MODULE, "Attempting to open /proc/meminfo");
    int fd = open("/proc/meminfo");
    if (fd < 0) {
        printf("free: failed to open /proc/meminfo\n");
        return 1;
    }
    
    char buf[512];
    int r = read(fd, buf, 511);
    if (r > 0) {
        buf[r] = '\0';
        printf("%s", buf);
        log_trace(MODULE, "Successfully read %d bytes from /proc/meminfo", r);
    }
    
    close(fd);
    log_debug(MODULE, "Closed /proc/meminfo file descriptor");
    return 0;
}