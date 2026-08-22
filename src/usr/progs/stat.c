#include "../libs/meow_libc.h"

DESCRIPTION("stat.elf: Show file metadata");

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: stat <file>\n");
        return 1;
    }

    sys_stat_t st;
    if (stat(argv[1], &st) != 0) {
        perror("stat");
        return 1;
    }

    printf("File: %s\n", argv[1]);
    printf("Size: %d bytes\n", st.size);
    printf("Type: %d\n", st.type);
    printf("UID: %d  GID: %d\n", st.uid, st.gid);
    printf("Mode: 0%o\n", st.mode);

    return 0;
}