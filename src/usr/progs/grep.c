#include "../libs/meow_libc.h"

#define MODULE "GREP"
#define LINE_BUFFER_SIZE 1024

DESCRIPTION("grep.elf: Print lines matching a pattern");

extern void log_trace(const char *module, const char *fmt, ...);

int main(int argc, char **argv) {
    log_trace(MODULE, "Invoked with %d arguments", argc);

    if (argc < 2) {
        printf("Usage: grep <pattern> [file]\n");
        return 1;
    }

    const char *pattern = argv[1];
    const char *filename = (argc >= 3) ? argv[2] : NULL;

    log_trace(MODULE, "Searching for pattern '%s'", pattern);

    int fd = 0;
    if (filename != NULL && strcmp(filename, "-") != 0) {
        log_trace(MODULE, "Opening target file '%s'", filename);
        fd = open(filename);
        if (fd < 0) {
            perror("grep: open failed");
            return 1;
        }
    } else {
        log_trace(MODULE, "Filtering stream from standard input");
    }

    char *line = (char *)malloc(LINE_BUFFER_SIZE);
    if (!line) {
        printf("grep: out of memory allocating line buffer\n");
        if (fd > 0) close(fd);
        return 1;
    }

    int matched_lines = 0;

    while (fgets(line, LINE_BUFFER_SIZE, fd) != NULL) {
        if (strstr(line, pattern) != NULL) {
            fputs(line, 1);
            matched_lines++;
        }
    }

    free(line);
    if (fd > 0) {
        close(fd);
    }

    log_trace(MODULE, "Finished grep. Matched %d lines", matched_lines);
    return (matched_lines > 0) ? 0 : 1;
}