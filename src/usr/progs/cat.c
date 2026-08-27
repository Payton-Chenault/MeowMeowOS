#include "../libs/meow_libc.h"

#define MODULE "CAT"
#define STREAM_BUFFER_SIZE 4096

DESCRIPTION("cat.elf: Print file contents or stream standard input");

extern void log_trace(const char *module, const char *fmt, ...);

static void stream_fd(int fd, const char *source_name) {
    char *buffer = (char *)malloc(STREAM_BUFFER_SIZE);
    if (!buffer) {
        printf("cat: out of memory allocating streaming buffer\n");
        return;
    }

    int bytes_read;
    int total_bytes = 0;

    while ((bytes_read = read(fd, buffer, STREAM_BUFFER_SIZE)) > 0) {
        total_bytes += bytes_read;
        int written = 0;
        while (written < bytes_read) {
            int w = write(1, buffer + written, bytes_read - written);
            if (w <= 0) {
                break;
            }
            written += w;
        }
    }

    log_trace(MODULE, "Streamed %d total bytes from %s", total_bytes, source_name);
    free(buffer);
}

int main(int argc, char **argv) {
    log_trace(MODULE, "Invoked with %d arguments", argc);

    if (argc < 2) {
        log_trace(MODULE, "Streaming from standard input");
        stream_fd(0, "stdin");
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-") == 0) {
            log_trace(MODULE, "Streaming from standard input via '-'");
            stream_fd(0, "stdin");
            continue;
        }

        log_trace(MODULE, "Opening file '%s'", argv[i]);
        int fd = open(argv[i]);
        if (fd < 0) {
            perror("cat: open failed");
            continue;
        }

        stream_fd(fd, argv[i]);
        close(fd);
    }

    return 0;
}