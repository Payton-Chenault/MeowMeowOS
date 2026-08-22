#include "../libs/meow_libc.h"

#define DEFAULT_TAIL_SIZE 512

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: tail <file> [bytes]\n");
        return 1;
    }

    const char *filename = argv[1];
    int tail_size = (argc > 2) ? atoi(argv[2]) : DEFAULT_TAIL_SIZE;
    if (tail_size < 1) tail_size = 1;

    int fd = open(filename);
    if (fd < 0) {
        perror("tail");
        return 1;
    }

    sys_stat_t st;
    if (fstat(fd, &st) != 0) {
        perror("tail");
        close(fd);
        return 1;
    }

    long start = (long)st.size - tail_size;
    if (start < 0) start = 0;

    if (lseek(fd, start, SEEK_SET) < 0) {
        perror("tail");
        close(fd);
        return 1;
    }

    char buffer[512];
    int bytes = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("%s", buffer);
    }

    close(fd);
    return 0;
}