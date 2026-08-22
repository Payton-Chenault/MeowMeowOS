#include "../libs/meow_libc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: cat <filename>\n");
        return 1;
    }

    int fd = open(argv[1]);
    if (fd < 0) {
        perror("cat");
        return 1;
    }

    char buffer[256];
    int bytes_read;

    while ((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }

    close(fd);
    printf("\n");
    return 0;
}