#include "../libs/meow_libc.h"

#define MODULE "HEAD"
#define DEFAULT_LINES 10
#define LINE_BUFFER_SIZE 1024

DESCRIPTION("head.elf: Output the first part of files");

int main(int argc, char **argv) {
    log_trace(MODULE, "Invoked with %d arguments", argc);

    if (argc < 2) {
        printf("Usage: head [-n lines] <file>\n");
        return 1;
    }

    int lines_to_print = DEFAULT_LINES;
    const char *filename = NULL;

    // Parse arguments: head -n 20 file.txt OR head file.txt 20 OR head file.txt
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

    log_trace(MODULE, "Target: %s, max lines: %d", filename, lines_to_print);

    int fd = open(filename);
    if (fd < 0) {
        perror("head: open failed");
        return 1;
    }

    char *line = (char *)malloc(LINE_BUFFER_SIZE);
    if (!line) {
        printf("head: failed to allocate line buffer\n");
        close(fd);
        return 1;
    }

    int count = 0;
    while (count < lines_to_print && fgets(line, LINE_BUFFER_SIZE, fd) != NULL) {
        fputs(line, 1);
        count++;
    }

    free(line);
    close(fd);
    log_trace(MODULE, "Finished head. Outputted %d lines total", count);
    return 0;
}