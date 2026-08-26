#include "../libs/meow_libc.h"

#define MODULE "TAIL"
#define DEFAULT_LINES 10
#define LINE_BUFFER_SIZE 1024

DESCRIPTION("tail.elf: Output the last part of files");

extern void log_trace(const char *module, const char *fmt, ...);

int main(int argc, char **argv) {
    log_trace(MODULE, "Invoked with %d arguments", argc);

    if (argc < 2) {
        printf("Usage: tail [-n lines] <file>\n");
        return 1;
    }

    int lines_to_print = DEFAULT_LINES;
    const char *filename = NULL;

    // Parse arguments: tail -n 20 file.txt OR tail file.txt 20 OR tail file.txt
    if (argc >= 4 && strcmp(argv[1], "-n") == 0) {
        lines_to_print = atoi(argv[2]);
        filename = argv[3];
    } else if (argc >= 3 && strcmp(argv[1], "-n") != 0) {
        filename = argv[1];
        lines_to_print = atoi(argv[2]);
    } else {
        filename = argv[1];
    }

    if (lines_to_print < 1) {
        lines_to_print = DEFAULT_LINES;
    }

    log_trace(MODULE, "Target: %s, trailing lines: %d", filename, lines_to_print);

    int fd = open(filename);
    if (fd < 0) {
        perror("tail: open failed");
        return 1;
    }

    // Allocate circular ring buffer on the heap
    char **ring_buffer = (char **)calloc(lines_to_print, sizeof(char *));
    if (!ring_buffer) {
        printf("tail: out of memory allocating line buffer\n");
        close(fd);
        return 1;
    }

    char line_buf[LINE_BUFFER_SIZE];
    int total_lines = 0;

    // Stream lines sequentially to prevent FAT16 lseek offset issues
    while (fgets(line_buf, sizeof(line_buf), fd) != NULL) {
        int slot = total_lines % lines_to_print;
        if (ring_buffer[slot] != NULL) {
            free(ring_buffer[slot]);
        }
        ring_buffer[slot] = strdup(line_buf);
        total_lines++;
    }

    close(fd);
    log_trace(MODULE, "Read %d total lines", total_lines);

    // Output buffered lines in FIFO order
    int count = (total_lines < lines_to_print) ? total_lines : lines_to_print;
    int start = (total_lines > lines_to_print) ? (total_lines % lines_to_print) : 0;

    for (int i = 0; i < count; i++) {
        int slot = (start + i) % lines_to_print;
        if (ring_buffer[slot] != NULL) {
            fputs(ring_buffer[slot], 1);
            free(ring_buffer[slot]);
            ring_buffer[slot] = NULL;
        }
    }

    free(ring_buffer);
    log_trace(MODULE, "Completed tail operation successfully");
    return 0;
}