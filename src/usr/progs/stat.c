#include "../libs/meow_libc.h"

#define MODULE "STAT"

DESCRIPTION("stat.elf: Show file or stream metadata");

extern void log_trace(const char *module, const char *fmt, ...);

int main(int argc, char **argv) {
    log_trace(MODULE, "Invoked with %d arguments", argc);

    sys_stat_t st;

    if (argc < 2) {
        log_trace(MODULE, "Querying status of standard input (FD 0)");
        if (fstat(0, &st) != 0) {
            perror("stat: fstat failed on stdin");
            return 1;
        }
        printf("Stream: stdin\n");
    } else {
        log_trace(MODULE, "Querying metadata for path '%s'", argv[1]);
        if (stat(argv[1], &st) != 0) {
            perror("stat: open failed");
            return 1;
        }
        printf("File:   %s\n", argv[1]);
    }

    printf("--------------------------------------------------\n");
    printf("Size:   %u bytes\n", st.size);
    printf("Type:   %u\n", st.type);
    printf("Mode:   0%o\n", st.mode);
    printf("Owner:  UID: %u   GID: %u\n", st.uid, st.gid);
    printf("--------------------------------------------------\n");

    log_trace(MODULE, "Stat metadata inspection complete");
    return 0;
}