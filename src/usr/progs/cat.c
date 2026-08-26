#include "../libs/meow_libc.h"

#define MODULE "CAT"
#define STREAM_BUFFER_SIZE 4096

DESCRIPTION("cat.elf: Print file contents");

extern void log_trace(const char *module, const char *fmt, ...);

int main(int argc, char **argv) {
    log_trace(MODULE, "Invoked with %d arguments", argc);

    if (argc < 2) {
        printf("\033[1;31mUsage:\033[0m cat <filename>\n");
        return 1;
    }

    log_trace(MODULE, "Opening file '%s'", argv[1]);
    int fd = open(argv[1]);
    if (fd < 0) {
        perror("\033[1;31mcat: open failed\033[0m");
        return 1;
    }

    char *buffer = (char *)malloc(STREAM_BUFFER_SIZE);
    if (!buffer) {
        printf("\033[1;31mcat: out of memory allocating streaming buffer\033[0m\n");
        close(fd);
        return 1;
    }

    int bytes_read;
    int total_bytes = 0;

    while ((bytes_read = read(fd, buffer, STREAM_BUFFER_SIZE - 1)) > 0) {
        total_bytes += bytes_read;
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }

    log_trace(MODULE, "Streamed %d total bytes from %s", total_bytes, argv[1]);

    free(buffer);
    close(fd);
    printf("\n");
    return 0;
}