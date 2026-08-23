#include "../libs/meow_libc.h"

#define BUFFER_SIZE 16384

DESCRIPTION("Print or control the kernel ring buffer");

static char log_buffer[BUFFER_SIZE + 1];

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    int bytes_read = sys_syslog(log_buffer, BUFFER_SIZE);
    
    if (bytes_read < 0) {
        printf("dmesg: failed to read syslog\n");
        return 1;
    }
    
    if (bytes_read == 0) {
        // Nothing logged yet
        return 0;
    }
    
    log_buffer[bytes_read] = '\0';
    
    // Simple state machine to strip ANSI color codes for VGA output
    int in_ansi = 0;
    for (int i = 0; i < bytes_read; i++) {
        if (log_buffer[i] == '\033') { // Escape character
            in_ansi = 1;
            continue;
        }
        
        if (in_ansi) {
            // ANSI color codes end with 'm' (e.g., \033[36m)
            if (log_buffer[i] == 'm') {
                in_ansi = 0;
            }
            continue;
        }
        
        putchar(log_buffer[i]);
    }
    
    return 0;
}