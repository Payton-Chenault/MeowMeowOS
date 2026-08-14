#include "../libs/meow_libc.h"

int _start(int argc, char** argv) {
    if (argc < 2) {
        sys_print("Usage: cat <filename>\n");
        sys_exit();
        return 1;
    }

    int fd = sys_open(argv[1]);
    if (fd < 0) {
        sys_print("cat: ");
        sys_print(argv[1]);
        sys_print(": No such file or directory\n");
        sys_exit();
        return 1;
    }

    char buffer[256];
    int bytes_read;
    int total_read = 0;

    while ((bytes_read = sys_read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        sys_print(buffer);
        total_read += bytes_read;
    }

    sys_close(fd);

    // Add newline only if we printed something and it didn't already end with one
    if (total_read > 0) {
        sys_print("\n");
    }

    sys_exit();
    return 0;
}