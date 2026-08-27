#include "../libs/meow_libc.h"

#define MODULE "UPTIME"

DESCRIPTION("uptime.elf: Show system uptime");

extern void log_trace(const char *module, const char *fmt, ...);
extern void log_debug(const char *module, const char *fmt, ...);
extern void log_info(const char *module, const char *fmt, ...);

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    log_info(MODULE, "Starting system uptime check...");

    log_debug(MODULE, "Attempting to open /proc/uptime");
    int fd = open("/proc/uptime");
    if (fd < 0) {
        printf("uptime: failed to open /proc/uptime\n");
        return 1;
    }
    
    char buf[64];
    int r = read(fd, buf, 63);
    if (r > 0) {
        buf[r] = '\0';
        printf("%s", buf);
        log_trace(MODULE, "Successfully read uptime ticks from /proc/uptime: %s", buf);
    }
    
    close(fd);
    log_debug(MODULE, "Closed /proc/uptime file descriptor");
    return 0;
}