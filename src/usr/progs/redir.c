#include "../libs/meow_libc.h"

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    const char *outfile = "redirect.txt";

    int fd = sys_create(outfile);
    if (fd < 0) {
        perror("redirect");
        return 1;
    }

    // Duplicate our file descriptor onto stdout (fd 1)
    if (dup2(fd, 1) < 0) {
        perror("dup2");
        close(fd);
        return 1;
    }

    close(fd);

    // This will now be written to redirect.txt
    printf("This line goes to the file.\n");
    puts("So does this one.");

    return 0;
}