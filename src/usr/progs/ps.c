#include "../libs/meow_libc.h"

#define MODULE "PS"

DESCRIPTION("ps.elf: Report a snapshot of the current processes");

extern void log_trace(const char *module, const char *fmt, ...);
extern void log_debug(const char *module, const char *fmt, ...);
extern void log_info(const char *module, const char *fmt, ...);

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    log_info(MODULE, "Starting process snapshot...");

    log_debug(MODULE, "Attempting to open /proc/tasks to retrieve active task snapshot");
    int fd = open("/proc/tasks");
    if (fd < 0) {
        printf("ps: failed to open /proc/tasks\n");
        return 1;
    }
    
    char buf[1024];
    int r;
    int total_bytes = 0;
    while ((r = read(fd, buf, 1023)) > 0) {
        buf[r] = '\0';
        printf("%s", buf);
        total_bytes += r;
        log_trace(MODULE, "Read chunk of %d bytes from /proc/tasks", r);
    }
    
    log_info(MODULE, "Completed reading %d total bytes from /proc/tasks", total_bytes);
    close(fd);
    return 0;
}