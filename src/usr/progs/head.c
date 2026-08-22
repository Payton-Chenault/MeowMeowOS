#include "../libs/meow_libc.h"

#define DEFAULT_LINES 10

DESCRIPTION("head.elf: Print first lines of a file");

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: head [-n lines] <file>\n");
        return 1;
    }

    int lines = DEFAULT_LINES;
    const char *filename = argv[1];

    if (argc >= 4 && strcmp(argv[1], "-n") == 0) {
        lines = atoi(argv[2]);
        if (lines < 1) lines = 1;
        filename = argv[3];
    }

    int fd = open(filename);
    if (fd < 0) {
        perror("head");
        return 1;
    }

    char line[256];
    int count = 0;
    while (count < lines && fgets(line, sizeof(line), fd) != NULL) {
        fputs(line, 1);
        count++;
    }

    close(fd);
    return 0;
}