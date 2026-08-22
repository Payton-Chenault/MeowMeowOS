#include "../libs/meow_libc.h"

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    const char *test_file = "dsk_test.txt";
    const char *test_data = "MeowMeowOS Disk Test Data!";
    char read_buf[64];

    int fd = sys_create(test_file);
    if (fd < 0) {
        perror("create");
        return 1;
    }
    close(fd);

    fd = open(test_file);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    write(fd, test_data, strlen(test_data));
    close(fd);

    fd = open(test_file);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    int bytes = read(fd, read_buf, sizeof(read_buf) - 1);
    read_buf[bytes] = '\0';
    close(fd);

    printf("%s\n", read_buf);

    unlink(test_file);
    return 0;
}