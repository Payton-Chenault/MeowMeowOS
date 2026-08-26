#include "../libs/meow_libc.h"

#define MODULE "TAIL"
#define DEFAULT_LINES 10
#define LINE_BUFFER_SIZE 1024

DESCRIPTION("tail.elf: Output the last part of files");

int main(int argc, char **argv) {
    int lines_to_print = DEFAULT_LINES;
    const char *filename = NULL;

    if (argc >= 3 && strcmp(argv[1], "-n") == 0) {
        lines_to_print = atoi(argv[2]);
        if (argc >= 4) {
            filename = argv[3];
        }
    } else if (argc >= 3 && strcmp(argv[1], "-n") != 0) {
        filename = argv[1];
        lines_to_print = atoi(argv[2]);
    } else if (argc == 2) {
        filename = argv[1];
    }

    if (lines_to_print < 1) {
        lines_to_print = DEFAULT_LINES;
    }

    int fd = 0;
    if (filename != NULL && strcmp(filename, "-") != 0) {
        fd = open(filename);
        if (fd < 0) {
            printf("tail: cannot open '%s'\n", filename);
            return 1;
        }
    }

    // Allocate ring buffer array
    char **ring_buffer = (char **)calloc(lines_to_print, sizeof(char *));
    if (!ring_buffer) {
        printf("tail: out of memory allocating line buffer\n");
        if (fd > 0) close(fd);
        return 1;
    }

    char line_buf[LINE_BUFFER_SIZE];
    int total_lines = 0;

    // Read stream sequentially line-by-line
    while (fgets(line_buf, sizeof(line_buf), fd) != NULL) {
        int slot = total_lines % lines_to_print;
        if (ring_buffer[slot] != NULL) {
            free(ring_buffer[slot]);
        }
        ring_buffer[slot] = strdup(line_buf);
        total_lines++;
    }

    if (fd > 0) {
        close(fd);
    }

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
    return 0;
}