#include "../libs/meow_libc.h"

#define MODULE "DMESG"
#define LOG_ALLOC_SIZE 32768

DESCRIPTION("Print or control the kernel ring buffer");

extern void log_trace(const char *module, const char *fmt, ...);

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    log_trace(MODULE, "Requesting kernel syslog buffer (%u bytes)", LOG_ALLOC_SIZE);

    char *log_buffer = (char *)malloc(LOG_ALLOC_SIZE + 1);
    if (!log_buffer) {
        printf("\033[1;31mdmesg: out of memory allocating syslog buffer\033[0m\n");
        return 1;
    }

    int bytes_read = sys_syslog(log_buffer, LOG_ALLOC_SIZE);
    log_trace(MODULE, "sys_syslog returned %d bytes", bytes_read);
    
    if (bytes_read < 0) {
        printf("\033[1;31mdmesg: failed to read syslog\033[0m\n");
        free(log_buffer);
        return 1;
    }
    
    if (bytes_read == 0) {
        printf("\033[1;33m[dmesg: log buffer is empty]\033[0m\n");
        free(log_buffer);
        return 0;
    }
    
    log_buffer[bytes_read] = '\0';
    
    // Stream filtered kernel log output
    int in_ansi = 0;
    for (int i = 0; i < bytes_read; i++) {
        if (log_buffer[i] == '\033') {
            in_ansi = 1;
            continue;
        }
        
        if (in_ansi) {
            if (log_buffer[i] == 'm') {
                in_ansi = 0;
            }
            continue;
        }
        
        putchar(log_buffer[i]);
    }

    free(log_buffer);
    log_trace(MODULE, "dmesg output rendered successfully");
    return 0;
}