#include "../libs/meow_libc.h"

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    char cwd[256];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("pwd");
        return 1;
    }

    printf("%s\n", cwd);
    return 0;
}