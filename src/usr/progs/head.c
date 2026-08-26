#include "../libs/meow_libc.h"

#define MODULE "HEAD"
#define DEFAULT_LINES 10
#define LINE_BUFFER_SIZE 1024

DESCRIPTION("head.elf: Output the first part of files");

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: head [-n lines] <file>\n");
        return 1;
    }

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
    } else {
        filename = argv[1];
    }

    if (lines_to_print < 1) {
        lines_to_print = DEFAULT_LINES;
    }

    int fd = 0;
    if (filename != NULL && strcmp(filename, "-") != 0) {
        fd = open(filename);
        if (fd < 0) {
            printf("head: cannot open '%s'\n", filename);
            return 1;
        }
    }

    char *line = (char *)malloc(LINE_BUFFER_SIZE);
    if (!line) {
        printf("head: out of memory\n");
        if (fd > 0) close(fd);
        return 1;
    }

    int count = 0;
    while (count < lines_to_print && fgets(line, LINE_BUFFER_SIZE, fd) != NULL) {
        fputs(line, 1);
        count++;
    }

    free(line);
    if (fd > 0) {
        close(fd);
    }
    return 0;
}