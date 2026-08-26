#include "../libs/meow_libc.h"

#define BUFFER_SIZE 16384
#define CHUNK_SIZE 512

DESCRIPTION("dmesg.elf: Print or control the kernel ring buffer");

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
        return 0;
    }
    
    log_buffer[bytes_read] = '\0';

    int total_written = 0;
    while (total_written < bytes_read) {
        int remaining = bytes_read - total_written;
        int write_len = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
        
        int written = write(1, log_buffer + total_written, write_len);
        if (written <= 0) {
            break; // Pipe closed or broken stream
        }
        total_written += written;
    }
    
    return 0;
}